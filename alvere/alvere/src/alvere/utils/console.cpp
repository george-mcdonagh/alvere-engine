#include "alvere/utils/console.hpp"

#include <sstream>

#include <glad/gl.h>

#include "graphics_api/opengl/opengl_errors.hpp"
#include "graphics_api/opengl/opengl_vertex_array.hpp"

#include "alvere/application/window.hpp"
#include "alvere/graphics/buffers.hpp"
#include "alvere/graphics/shader.hpp"
#include "alvere/graphics/shader_program.hpp"
#include "alvere/graphics/sprite_batcher.hpp"
#include "alvere/graphics/text/font.hpp"
#include "alvere/graphics/texture.hpp"
#include "alvere/math/matrix/matrix_4.hpp"
#include "alvere/math/matrix/transformations.hpp"
#include "alvere/utils/utilities.hpp"

namespace alvere::console
{
	bool Command::Param::validateArg(const Arg * arg, std::string & output) const
	{
		if (arg->getTypeIndex() != m_typeIndex)
		{
			output = "Argument types are not compatible.";
			return false;
		}
		return true;
	}

	Command::Param::Param(const std::string & name, const std::string & description, bool isRequired, std::type_index typeIndex, const char * typeString)
		: m_name(name), m_description(description), m_isRequired(isRequired), m_typeIndex(typeIndex)
	{
		m_detailedName = isRequired ? "<()>" : "[()]";
		m_detailedName.insert(2, typeString);
		m_detailedName.insert(1, m_name);
	}

	Command::Command(const char * name, const char * description, std::vector<Param *> params, Function f)
		: m_name(name), m_description(description), m_params(params), m_f(f)
	{

	}

	std::vector<const Command *> _commands;

	namespace gui
	{
		bool _initialised = false;
		bool _shown = false;

		Asset<ShaderProgram> _shaderProgram;
		VertexBuffer * _vbo;
		graphics_api::opengl::VertexArray * _vao;
		Matrix4 _projection;

		Asset<SpriteBatcher> _spriteBatcher;
		Font::Face _fontFace("res/fonts/consola.ttf");
		const Font::Face::Bitmap * _fontFaceBitmap;
		unsigned int _fontSize = 18;
		unsigned int _maxOutputLineCount = 9;

		Window * _window;
		CharInputEvent::Handler _charInputEventHandler;

		Key _keyShowHide = Key::BackQuote;
		Key _keySubmitCommand = Key::Enter;
		Key _keyHistoryBack = Key::Up;
		Key _keyHistoryForward = Key::Down;
		Key _keyCaretLeft = Key::Left;
		Key _keyCaretRight = Key::Right;
		Key _keyEraseCharLeft = Key::BackSpace;
		Key _keyEraseCharRight = Key::Delete;
		Key _keyAutoFill = Key::Tab;
		Key _keyAutoFillAccept = Key::Space;
		Key _keyPageUp = Key::PageUp;
		Key _keyPageDown = Key::PageDown;

		std::vector<std::string> _output;
		unsigned int _outputPageIndex = 0;
		std::vector<std::string> _inputHistory;
		std::string _inputPre = "alvere:>";
		std::string _input;
		unsigned int _caretPosition;
		bool _caretIsVisible = true;
		float _caretFlashSpeed = 0.5f;
		float _timeUntilCaretToggle = _caretFlashSpeed;

		//Command _helpCommand;

		void submitInput();
		void viewHistoryBack();
		void viewHistoryForward();
		void moveCaretLeft();
		void moveCaretRight();
		void eraseCharLeft();
		void eraseCharRight();
		void tryViewAutoFill();
		void tryAcceptAutoFill();
		void pageUpOutput();
		void pageDownOutput();
		void onCharInput(unsigned int);
		void clearInput();
		void resetCaretVisibility();

		void init(Window * window)
		{
			if (_initialised)
				return;

			_fontFaceBitmap = _fontFace.getBitmap(_fontSize);

			Asset<Shader> vShader = Shader::New(Shader::Type::Vertex, R"(#version 330 core
					uniform mat4 u_projectionMatrix;

					layout(location = 0) in vec3 a_position;

					void main()
					{
						gl_Position = u_projectionMatrix * vec4(a_position, 1.0f);
					}
				)");

			Asset<Shader> fShader = Shader::New(Shader::Type::Fragment, R"(#version 330 core
					uniform vec3 u_colour;
					uniform int u_lineHeight;
					uniform int u_outputLineCount;

					out vec4 f_colour;

					void main()
					{
						int y = int(gl_FragCoord.y);

						if (y > u_lineHeight * (u_outputLineCount + 1))
							discard;

						if (y <= u_lineHeight)
							f_colour = vec4(u_colour, 0.85f);
						else
							f_colour = vec4(u_colour, 0.5f);
					}
				)");

			_shaderProgram = ShaderProgram::New();
			_shaderProgram->SetShader(AssetRef<Shader>(vShader.get()));
			_shaderProgram->SetShader(AssetRef<Shader>(fShader.get()));
			_shaderProgram->build();

			_window = window;
			_charInputEventHandler.setFunction(onCharInput);
			_window->getEvent<CharInputEvent>()->subscribe(_charInputEventHandler);

			unsigned int width = _window->getWidth();
			unsigned int height = _window->getHeight();

			_projection = orthographic(0, width, height, 0, -1, 1);

			_shaderProgram->bind();
			_shaderProgram->sendUniformMat4x4("u_projectionMatrix", _projection);
			_shaderProgram->sendUniformFloat3("u_colour", 0.0f, 0.0f, 0.0f);
			_shaderProgram->sendUniformInt1("u_lineHeight", _fontFaceBitmap->getFontFaceHeight());
			_shaderProgram->sendUniformInt1("u_outputLineCount", _maxOutputLineCount);

			float vertexData[18] = {
				0.0f, height, 0.0f,
				width, 0.0f, 0.0f,
				0.0f, 0.0f, 0.0f,

				0.0f, height, 0.0f,
				width, height, 0.0f,
				width, 0.0f, 0.0f,
			};

			_vbo = VertexBuffer::New(vertexData, sizeof(vertexData));
			_vbo->SetLayout(BufferLayout({
				BufferElementProperties(Shader::DataType::Float3, "a_position", false)
				}));

			_vao = new graphics_api::opengl::VertexArray;
			_vao->AddVertexBuffer(_vbo);


			_spriteBatcher = SpriteBatcher::New();

			_initialised = true;

			_output.emplace_back("Type 'help' for a list of available commands.");
		}

		void destroy()
		{
			delete _vbo;
			delete _vao;

			_initialised = false;
		}

		void show()
		{
			if (!_initialised)
				return;

			_shown = true;

			clearInput();

			_outputPageIndex = 0;
		}

		void hide()
		{
			if (!_shown)
				return;

			_shown = false;
		}

		void update(float deltaTime)
		{
			if (_window->getKey(_keyShowHide).justPressed)
			{
				if (_shown)
				{
					hide();
					return;
				}
				else show();
			}

			if (!_shown)
				return;

			if ((_timeUntilCaretToggle -= deltaTime) <= 0.0f)
			{
				_timeUntilCaretToggle = _caretFlashSpeed;
				_caretIsVisible = !_caretIsVisible;
			}

			if (_window->getKey(_keySubmitCommand).justPressed)
				submitInput();
			else if (_window->getKey(_keyHistoryBack).justPressed || _window->getKey(_keyHistoryBack).isRepeating)
				viewHistoryBack();
			else if (_window->getKey(_keyHistoryForward).justPressed || _window->getKey(_keyHistoryForward).isRepeating)
				viewHistoryForward();
			else if (_window->getKey(_keyCaretLeft).justPressed || _window->getKey(_keyCaretLeft).isRepeating)
				moveCaretLeft();
			else if (_window->getKey(_keyCaretRight).justPressed || _window->getKey(_keyCaretRight).isRepeating)
				moveCaretRight();
			else if (_window->getKey(_keyEraseCharLeft).justPressed || _window->getKey(_keyEraseCharLeft).isRepeating)
				eraseCharLeft();
			else if (_window->getKey(_keyEraseCharRight).justPressed || _window->getKey(_keyEraseCharRight).isRepeating)
				eraseCharRight();
			else if (_window->getKey(_keyAutoFill).justPressed || _window->getKey(_keyAutoFill).isRepeating)
				tryViewAutoFill();
			else if (_window->getKey(_keyAutoFillAccept).justPressed)
				tryAcceptAutoFill();
			else if (_window->getKey(_keyPageUp).justPressed || _window->getKey(_keyPageUp).isRepeating)
				pageUpOutput();
			else if (_window->getKey(_keyPageDown).justPressed || _window->getKey(_keyPageDown).isRepeating)
				pageDownOutput();
		}

		void draw()
		{
			if (!_shown)
				return;

			float inputXOffset = _fontFaceBitmap->getTextSize(_inputPre).x;
			float caretXOffset = _fontFaceBitmap->getTextSize(_input.substr(0, _caretPosition)).x;

			_spriteBatcher->begin(_projection);

			_spriteBatcher->submit(*_fontFaceBitmap, _inputPre.c_str(), Vector2(3.0f, 6.0f));
			_spriteBatcher->submit(*_fontFaceBitmap, _input.c_str(), Vector2(3.0f + inputXOffset, 6.0f));
			
			if (_caretIsVisible)
				_spriteBatcher->submit(*_fontFaceBitmap, "_", Vector2(3.0f + inputXOffset + caretXOffset, 6.0f));

			int lineCount = 0;
			for (int i = _outputPageIndex * _maxOutputLineCount; i < _output.size() && lineCount < _maxOutputLineCount; ++i, ++lineCount)
			{
				_spriteBatcher->submit(*_fontFaceBitmap, _output[_output.size() - 1 - i], Vector2(3.0f, 6.0f + _fontFaceBitmap->getFontFaceHeight() * (lineCount + 1)));
			}

			_shaderProgram->bind();
			_shaderProgram->sendUniformInt1("u_outputLineCount", lineCount);
			_vao->Bind();
			ALV_LOG_OPENGL_CALL(glDrawArrays(GL_TRIANGLES, 0, 18));

			_spriteBatcher->end();
		}

		void submitInput()
		{
			_output.emplace_back(_inputPre + _input);
			_inputHistory.emplace_back(_input);

			std::string output = submitCommand(_input);

			if (!output.empty())
			{
				std::vector<std::string> lines = utils::splitString(output, '\n');
				_output.insert(_output.end(), lines.begin(), lines.end());
			}

			resetCaretVisibility();

			clearInput();
		}

		void viewHistoryBack()
		{
			// todo
		}

		void viewHistoryForward()
		{
			// todo
		}

		void moveCaretLeft()
		{
			resetCaretVisibility();

			if (_caretPosition > 0)
				--_caretPosition;
		}

		void moveCaretRight()
		{
			resetCaretVisibility();

			if (_caretPosition < _input.size())
				++_caretPosition;
		}

		void eraseCharLeft()
		{
			resetCaretVisibility();

			if (_caretPosition == 0)
				return;

			_input.erase(_input.begin() + _caretPosition - 1);

			moveCaretLeft();
		}

		void eraseCharRight()
		{
			resetCaretVisibility();

			if (_caretPosition == _input.size())
				return;

			_input.erase(_input.begin() + _caretPosition);
		}

		void tryViewAutoFill()
		{
			// todo: pressing tab will try and display the next possible auto fill value
		}

		void tryAcceptAutoFill()
		{
			// todo: pressing space when an autofilloption is displayed will accept the auto fill
		}

		void pageUpOutput()
		{
			if ((unsigned int)_output.size() - (_outputPageIndex * _maxOutputLineCount) > _maxOutputLineCount)
				++_outputPageIndex;
		}

		void pageDownOutput()
		{
			if (_outputPageIndex == 0)
				return;

			--_outputPageIndex;
		}

		void onCharInput(unsigned int utfCodePoint)
		{
			if (!_shown)
				return;

			resetCaretVisibility();

			_input.insert(_input.begin() + _caretPosition, (char)utfCodePoint);

			moveCaretRight();
		}

		void clearInput()
		{
			_input = "";
			_caretPosition = 0;

			resetCaretVisibility();
		}

		void resetCaretVisibility()
		{
			_caretIsVisible = true;
			_timeUntilCaretToggle = _caretFlashSpeed;
		}
	}

	void registerCommand(const Command & command)
	{
		auto iter = std::find(_commands.begin(), _commands.end(), &command);

		if (iter != _commands.end())
			return;

		_commands.emplace_back(&command);
	}

	void unregisterCommand(const Command & command)
	{
		auto iter = std::find(_commands.begin(), _commands.end(), &command);

		if (iter != _commands.end())
			return;

		_commands.erase(iter);
	}

	std::string submitCommand(const std::string & command)
	{
		return "";
	}
}
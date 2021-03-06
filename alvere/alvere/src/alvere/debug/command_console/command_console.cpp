#include "alvere/debug/command_console/command_console.hpp"

#include <algorithm>
#include <memory>
#include <regex>
#include <sstream>

#include <glad/gl.h>

#include "graphics_api/opengl/opengl_errors.hpp"
#include "graphics_api/opengl/opengl_vertex_array.hpp"

#include "alvere/application/window.hpp"
#include "alvere/debug/command_console/arg.hpp"
#include "alvere/debug/command_console/command.hpp"
#include "alvere/debug/command_console/command_alias.hpp"
#include "alvere/debug/command_console/param.hpp"
#include "alvere/graphics/buffers.hpp"
#include "alvere/graphics/shader.hpp"
#include "alvere/graphics/shader_program.hpp"
#include "alvere/graphics/sprite_batcher.hpp"
#include "alvere/graphics/text/font.hpp"
#include "alvere/graphics/text/text_display.hpp"
#include "alvere/graphics/texture.hpp"
#include "alvere/math/matrix/matrix_4.hpp"
#include "alvere/math/matrix/transformations.hpp"
#include "alvere/utils/utilities.hpp"

namespace alvere::console
{
	std::vector<const Command *> _commands;
	std::vector<std::unique_ptr<CommandAlias>> _aliasCommands;

	namespace gui
	{
		bool _initialised = false;
		bool _shown = false;

		std::unique_ptr<ShaderProgram> _shaderProgram;
		VertexBuffer * _vbo;
		graphics_api::opengl::VertexArray * _vao;
		Matrix4 _projection;

		std::unique_ptr<SpriteBatcher> _spriteBatcher;
		Font _font;
		unsigned int _fontSize = 14;
		unsigned int _maxOutputLineCountShrunk = 14;
		unsigned int _maxOutputLineCountExpanded = 48;
		unsigned int _maxOutputLineCount = _maxOutputLineCountShrunk;

		Window * _window;
		CharInputEvent::Handler _charInputEventHandler;

		/***********************************************************************************************************************/

		TextDisplay _outputTextDisplay;
		std::unique_ptr<CompositeText> _outputContent;
		Text::Formatting _defaultTextFormatting;

		/***********************************************************************************************************************/

		//std::vector<std::string> _output;
		unsigned int _outputPageIndex = 0;
		std::vector<std::string> _inputHistory;
		std::string _inputPre = "> ";
		std::string _input;
		std::string _inputSuggestion;
		std::vector<std::string> _suggestions;
		int _currentHistory = -1;
		unsigned int _caretPosition;
		bool _caretIsVisible = true;
		float _caretFlashSpeed = 0.5f;
		float _timeUntilCaretToggle = _caretFlashSpeed;

		Key _keyShowHide = Key::BackQuote;
		Key _keySubmitCommand = Key::Enter;
		Key _keyHistoryBack = Key::Up;
		Key _keyHistoryForward = Key::Down;
		Key _keyCaretLeft = Key::Left;
		Key _keyCaretRight = Key::Right;
		Key _keyEraseCharLeft = Key::BackSpace;
		Key _keyEraseCharRight = Key::Delete;
		Key _keyApplySuggestion = Key::Tab;
		Key _keyAcceptSuggestion = Key::Space;
		Key _keyPageUp = Key::PageUp;
		Key _keyPageDown = Key::PageDown;

		std::vector<std::unique_ptr<Command>> _builtInCommands;

		void submitInput();
		void viewHistoryBack();
		void viewHistoryForward();
		int getLeftSkipIndex();
		int getRightSkipIndex();
		void moveCaretLeft(bool skipWord = false);
		void moveCaretRight(bool skipWord = false);
		void eraseCharLeft(bool skipWord = false);
		void eraseCharRight(bool skipWord = false);
		void tryCycleSuggestions();
		void tryApplySuggestion();
		void pageUpOutput();
		void pageDownOutput();
		void onCharInput(unsigned int);
		void updateInputSuggestion();
		void clearInput();
		void resetCaretVisibility();

		void init(Window * window)
		{
			if (_initialised)
				return;

			_font.loadFontFaceFile("res/fonts/consolas/consola.ttf");
			_font.loadFontFaceFile("res/fonts/consolas/consolab.ttf");
			_font.loadFontFaceFile("res/fonts/consolas/consolai.ttf");
			_font.loadFontFaceFile("res/fonts/consolas/consolaz.ttf");

			_defaultTextFormatting = Text::Formatting{
				&_font,
				Font::Style::Regular,
				14,
				Vector4::unit
			};

			_builtInCommands.emplace_back(std::make_unique<Command>(
				"help",
				"Displays a list of all of the available commands, and can optionally be used to display further information about a single command.",
				std::vector<IParam *> {
				&StringParam("command name", "The name of the command to display information about.", false) },
				[&](std::vector<const IArg *> args) -> CompositeText
				{
					CompositeText output(_defaultTextFormatting);

					if (args[0] == nullptr)
					{
						output = "Below is a list of all of the available commands. To see more information about a particular command, type 'help' with the name of the command.\n";

						for (const Command * command : _commands)
							output += "\n" + command->getSignature() + " : " + command->getDescription();

						if (_aliasCommands.size() > 0)
						{
							output += "\n\nCommand aliases\n";

							for (const std::unique_ptr<CommandAlias> & alias : _aliasCommands)
								output += "\n" + alias->getSignature() + " : (" + alias->getCommandString() + ") " + alias->getDescription();
						}
					}
					else
					{
						bool commandFound = false;

						const std::string & commandName = args[0]->getValue<std::string>();

						const Command * foundCommand = nullptr;

						for (const Command * command : _commands)
							if (command->getName() == commandName)
							{
								output += command->getSignature() + "\n\t" + command->getDescription();

								const std::vector<IParam *> & params = command->getParams();

								for (const IParam * param : params)
									output += "\n\t" + param->getDetailedName() + " : " + param->getDescription();

								return output;
							}

						for (const std::unique_ptr<CommandAlias> & alias : _aliasCommands)
							if (alias->getName() == commandName)
							{
								output += alias->getSignature() + " (alias)\n\t(" + alias->getCommandString() + ")";

								const std::string & description = alias->getDescription();

								if (!description.empty())
									output += "\n\t" + description;

								return output;
							}

						output = "Command '" + commandName + "' not found. Type 'help' for a list of available commands.";
					}

					return output;
				}));
			
			_builtInCommands.emplace_back(std::make_unique<Command>(
				"console.clear",
				"Clears the console output.",
				std::vector<IParam *> {},
				[&](std::vector<const IArg *> args) -> CompositeText
			{
				_outputContent->clear();
				return CompositeText(_defaultTextFormatting);
			}));
			/*
			_builtInCommands.emplace_back(std::make_unique<Command>(
				"console.expand",
				"Expands the console window.",
				std::vector<IParam *> {},
				[&](std::vector<const IArg *> args) -> CompositeText
			{
				_maxOutputLineCount = _maxOutputLineCountExpanded;
				_shaderProgram->bind();
				_shaderProgram->sendUniformInt1("u_outputLineCount", _maxOutputLineCount);
				return "";
			}));

			_builtInCommands.emplace_back(std::make_unique<Command>(
				"console.shrink",
				"Shrinks the console window.",
				std::vector<IParam *> {},
				[&](std::vector<const IArg *> args) -> CompositeText
			{
				_maxOutputLineCount = _maxOutputLineCountShrunk;
				_shaderProgram->bind();
				_shaderProgram->sendUniformInt1("u_outputLineCount", _maxOutputLineCount);
				return "";
			}));
			*/

			_builtInCommands.emplace_back(std::make_unique<Command>(
				"alias.new",
				"Creates an alias for a specified command call.",
				std::vector<IParam *> {
				&StringParam("new alias name", "The name of the new command alias being created.", true),
					& StringParam("command string", "The command to be executed when the command alias is entered.", true),
					& StringParam("description", "A description of the new command alias.", false), },
				[&](std::vector<const IArg *> args) -> CompositeText
				{
					CompositeText output(_defaultTextFormatting);

					std::string name = args[0]->getValue<std::string>();

					if(!std::regex_match(name, std::regex("^[A-Za-z.-_]+$")))
					{
						output.append("Command alias bad format.");
						return output;
					}

					for (size_t i = 0; i < name.length(); ++i)
						name[i] = std::tolower(name[i]);

					for (const Command * command : _commands)
						if (command->getName() == name)
						{
							output.append("Failed to create alias. Command '" + name + "' already exists. Command alias names must be unique.");
							return output;
						}

					for (const std::unique_ptr<CommandAlias> & alias : _aliasCommands)
						if (alias->getName() == name)
						{
							output.append("Failed to create alias. Command alias '" + name + "' already exists. Command alias names must be unique.");
							return output;
						}

					std::string description = "";

					if (args[2] != nullptr)
						description = args[2]->getValue<std::string>();

					_aliasCommands.emplace_back(std::make_unique<CommandAlias>(
						name.c_str(),
						description.c_str(),
						args[1]->getValue<std::string>().c_str()));

					std::sort(_aliasCommands.begin(), _aliasCommands.end(),
						[](const std::unique_ptr<CommandAlias> & a, const std::unique_ptr<CommandAlias> & b) -> bool
					{
						return a->getName() < b->getName();
					});

					output.append("Successfully created new alias '" + name + "'.");
					return output;
				}));

			_builtInCommands.emplace_back(std::make_unique<Command>(
				"alias.delete",
				"Deletes an existing alias.",
				std::vector<IParam *> {
				&StringParam("alias name", "The name of the alias to delete.", true), },
				[&](std::vector<const IArg *> args) -> CompositeText
				{
					CompositeText output(_defaultTextFormatting);

					const std::string & name = args[0]->getValue<std::string>();

					for (size_t a = 0; a < _aliasCommands.size(); ++a)
						if (_aliasCommands[a]->getName() == name)
						{
							_aliasCommands.erase(_aliasCommands.begin() + a);
							output.append("Successfully deleted alias '" + name + "'.");
							return output;
						}

					output.append("Alias '" + name + "' does not exist.");
					return output;
				}));

			_builtInCommands.emplace_back(std::make_unique<Command>(
				"test",
				"This is a test command with all currently possible types of parameters.",
				std::vector<IParam *> {
					&BoolParam("test1", "A boolean parameter.", true),
					&UIntParam("test2", "An unsigned int parameter.", true),
					&IntParam("test3", "An int parameter.", true),
					&FloatParam("test4", "A float parameter.", true),
					&StringParam("test5", "A string parameter.", true),
					&OptionParam("test6", "An option parameter.", true, { "one", "two", "three" }),
					&EnumParam<Font::Style>("test7", "An enum parameter.", true) },
				[](std::vector<const IArg *> args) -> CompositeText
				{
					CompositeText output(_defaultTextFormatting);
					output.append("Test command!");
					return output;
				}));

			std::unique_ptr<Shader> vShader = Shader::New(Shader::Type::Vertex, R"(#version 330 core
					uniform mat4 u_projectionMatrix;

					layout(location = 0) in vec3 a_position;

					void main()
					{
						gl_Position = u_projectionMatrix * vec4(a_position, 1.0f);
					}
				)");

			std::unique_ptr<Shader> fShader = Shader::New(Shader::Type::Fragment, R"(#version 330 core
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
							f_colour = vec4(u_colour, 0.925f);
						else
							f_colour = vec4(u_colour, 0.8f);
					}
				)");

			_shaderProgram = ShaderProgram::New();
			_shaderProgram->SetShader(vShader.get());
			_shaderProgram->SetShader(fShader.get());
			_shaderProgram->build();

			_window = window;
			_charInputEventHandler.setFunction(onCharInput);
			_window->getEvent<CharInputEvent>()->subscribe(_charInputEventHandler);

			unsigned int width = _window->getSize().x;
			unsigned int height = _window->getSize().y;

			_projection = orthographic(0, width, height, 0, -1, 1);

			_shaderProgram->bind();
			_shaderProgram->sendUniformMat4x4("u_projectionMatrix", _projection);
			_shaderProgram->sendUniformFloat3("u_colour", 0.0f, 0.0f, 0.0f);
			_shaderProgram->sendUniformInt1("u_lineHeight", _font.getFontFace(Font::Style::Regular)->getBitmap(_fontSize)->getFontFaceHeight());
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

			_outputContent = std::make_unique<CompositeText>(_defaultTextFormatting, std::vector<Text>{ Text { _defaultTextFormatting, "Type 'help' for a list of available commands.\n" } });

			_outputTextDisplay.setContent(*_outputContent);
			_outputTextDisplay.setPosition(Vector2{ 3.0f, 6.0f + _outputContent->getContent()[0].getFontFaceBitmap()->getTextSize("Type 'help' for a list of available commands.").y });
			_outputTextDisplay.setWrapWidth(_window->getSize().x);
			_outputTextDisplay.setAnchor(TextDisplay::Anchor::Bottom);
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
				moveCaretLeft(_window->getKey(Key::LeftCtrl).isDown || _window->getKey(Key::RightCtrl).isDown);
			else if (_window->getKey(_keyCaretRight).justPressed || _window->getKey(_keyCaretRight).isRepeating)
				moveCaretRight(_window->getKey(Key::LeftCtrl).isDown || _window->getKey(Key::RightCtrl).isDown);
			else if (_window->getKey(_keyEraseCharLeft).justPressed || _window->getKey(_keyEraseCharLeft).isRepeating)
				eraseCharLeft(_window->getKey(Key::LeftCtrl).isDown || _window->getKey(Key::RightCtrl).isDown);
			else if (_window->getKey(_keyEraseCharRight).justPressed || _window->getKey(_keyEraseCharRight).isRepeating)
				eraseCharRight(_window->getKey(Key::LeftCtrl).isDown || _window->getKey(Key::RightCtrl).isDown);
			else if (_window->getKey(_keyApplySuggestion).justPressed || _window->getKey(_keyApplySuggestion).isRepeating)
				tryCycleSuggestions();
			else if (_window->getKey(_keyAcceptSuggestion).justPressed)
				tryApplySuggestion();
			else if (_window->getKey(_keyPageUp).justPressed || _window->getKey(_keyPageUp).isRepeating)
				pageUpOutput();
			else if (_window->getKey(_keyPageDown).justPressed || _window->getKey(_keyPageDown).isRepeating)
				pageDownOutput();
		}

		void draw()
		{
			// todo: most of this shit to be done by some sort of formatted text renderer

			if (!_shown)
				return;

			const Font::Face::Bitmap * bitmap = _font.getFontFace(Font::Style::Regular)->getBitmap(_fontSize);

			float inputXOffset = bitmap->getTextSize(_inputPre).x;
			float caretXOffset = bitmap->getTextSize(_input.substr(0, _caretPosition)).x;

			_spriteBatcher->begin(_projection);

			_outputTextDisplay.render(*_spriteBatcher);

			_spriteBatcher->submit(*bitmap, _inputPre.c_str(), Vector2(3.0f, 6.0f));

			_spriteBatcher->submit(*bitmap, _input.c_str(), Vector2(3.0f + inputXOffset, 6.0f));

			if (_caretIsVisible)
				_spriteBatcher->submit(*bitmap, "_", Vector2(3.0f + inputXOffset + caretXOffset, 6.0f));

			int lineCount = _outputTextDisplay.getLineCount();
			/*for (int i = _outputPageIndex * _maxOutputLineCount; i < _output.size() && lineCount < _maxOutputLineCount; ++i, ++lineCount)
			{
				_spriteBatcher->submit(*bitmap, _output[_output.size() - 1 - i], Vector2(3.0f, 6.0f + bitmap->getFontFaceHeight() * (lineCount + 1)));
			}*/

			if (lineCount > _maxOutputLineCount)
			{
				int pageCount = lineCount / _maxOutputLineCount + 1;
				std::string pageCounter = std::to_string(_outputPageIndex + 1) + "/" + std::to_string(pageCount);
				_spriteBatcher->submit(*bitmap, pageCounter, Vector2(_window->getSize().x - (3.0f + bitmap->getTextSize(pageCounter).x), 6.0f));
			}

			_shaderProgram->bind();
			_shaderProgram->sendUniformInt1("u_outputLineCount", lineCount < _maxOutputLineCount ? lineCount : _maxOutputLineCount);
			_vao->Bind();
			ALV_LOG_OPENGL_CALL(glDrawArrays(GL_TRIANGLES, 0, 18));

			_spriteBatcher->end();
		}

		void onWindowResize(int width, int height)
		{
			_projection = orthographic(0, width, height, 0, -1, 1);
			_outputTextDisplay.setWrapWidth(width);
		}

		Text::Formatting & defaultTextFormatting()
		{
			return _defaultTextFormatting;
		}

		void submitInput()
		{
			//_output.emplace_back(_inputPre + _input);
			_inputHistory.insert(_inputHistory.begin(), _input);

			_outputContent->append("\n" + _inputPre + _input + "\n");

			CompositeText output = submitCommand(_input);

			_outputContent->append(output);
			_outputContent->append("\n");
			_outputTextDisplay.setContent(*_outputContent);
			_outputTextDisplay.setLineRange(_outputTextDisplay.getLineCount() - _maxOutputLineCount, _outputTextDisplay.getLineCount() - 1);

			_outputPageIndex = 0;

			resetCaretVisibility();

			clearInput();
		}

		void viewHistoryBack()
		{
			if (_currentHistory == _inputHistory.size() - 1)
				return;

			++_currentHistory;

			_input = _inputHistory[_currentHistory];
			_caretPosition = _input.size();
			resetCaretVisibility();
		}

		int getLeftSkipIndex()
		{
			bool wasOnChar = false;
			size_t pos = _caretPosition;
			while (pos > 0)
			{
				if (wasOnChar && std::isspace(_input[pos - 1]))
					break;
				--pos;
				wasOnChar = !std::isspace(_input[pos]);
			}
			return pos;
		}

		int getRightSkipIndex()
		{
			bool wasOnSpace = false;
			int pos = _caretPosition;
			while (pos < _input.length())
			{
				if (wasOnSpace && !std::isspace(_input[pos]))
					break;
				wasOnSpace = std::isspace(_input[pos]);
				++pos;
			}
			return pos;
		}

		void viewHistoryForward()
		{
			if (_currentHistory == -1)
				return;

			if (_currentHistory == 0)
			{
				clearInput();
				return;
			}

			--_currentHistory;

			_input = _inputHistory[_currentHistory];
			_caretPosition = _input.size();
			resetCaretVisibility();
		}

		void moveCaretLeft(bool skipWord)
		{
			resetCaretVisibility();

			if (skipWord)
			{
				_caretPosition = getLeftSkipIndex();
			}
			else if (_caretPosition > 0)
			{
				--_caretPosition;
			}
		}

		void moveCaretRight(bool skipWord)
		{
			resetCaretVisibility();

			if (skipWord)
			{
				_caretPosition = getRightSkipIndex();
			}
			else if (_caretPosition < _input.length())
			{
				++_caretPosition;
			}
		}

		void eraseCharLeft(bool skipWord)
		{
			resetCaretVisibility();

			if (_caretPosition == 0)
				return;

			size_t caretOrigin = _caretPosition;

			moveCaretLeft(skipWord);

			_input.erase(_input.begin() + _caretPosition, _input.begin() + caretOrigin);
		}

		void eraseCharRight(bool skipWord)
		{
			resetCaretVisibility();

			if (_caretPosition == _input.size())
				return;

			auto eraseLast = skipWord ? _input.begin() + getRightSkipIndex() : _input.begin() + _caretPosition + 1;

			_input.erase(_input.begin() + _caretPosition, eraseLast);
		}

		void tryCycleSuggestions()
		{
			// todo: pressing tab will try and display the next available suggestion
		}

		void tryApplySuggestion()
		{
			// todo: pressing space when a suggestion is displayed will apply the suggestion to the input
		}

		void pageUpOutput()
		{
			if ((unsigned int)_outputTextDisplay.getLineCount() - (_outputPageIndex * _maxOutputLineCount) > _maxOutputLineCount)
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

			char c = (char)utfCodePoint;

			_input.insert(_input.begin() + _caretPosition, c);

			moveCaretRight();

			updateInputSuggestion();
		}

		void updateInputSuggestion()
		{
			if (_currentHistory > -1)
			{
				_inputSuggestion = _inputHistory[_currentHistory];
				return;
			}

			_suggestions.clear();



			// do we have a command name yet?


			// todo: try and find suggestions for command names and paameters

			if (_suggestions.size() > 0)
			{
				// todo: set suggestion thing
			}
		}

		void clearInput()
		{
			_input = "";
			_caretPosition = 0;
			_currentHistory = -1;

			resetCaretVisibility();
		}

		void resetCaretVisibility()
		{
			_caretIsVisible = true;
			_timeUntilCaretToggle = _caretFlashSpeed;
		}
	}

	CompositeText submitCommand(const std::string & command)
	{
		std::vector<std::string> parts;
		size_t partBegin;
		char delim(0);

		CompositeText output(Text::Formatting{ &gui::_font, Font::Style::Regular, 14, Vector4::unit });

		for (size_t i = 0; i < command.length(); ++i)
		{
			if (std::isspace(command[i]))
				continue;

			delim = 0;
			partBegin = i;

			if (command[i] == '"' || command[i] == '\'')
			{
				++partBegin;

				if (partBegin >= command.length())
				{
					return output = std::string("Expecting matching delimiter (") + command[i] + ")";
				}

				delim = command[i];
			}

			for (size_t j = partBegin; ; ++j)
			{
				if (delim)
				{
					if (j >= command.length())
					{
						return output = std::string("Expecting matching delimiter (") + command[i] + ")";
					}
					else if (command[j] == delim)
					{
						parts.emplace_back(command.substr(partBegin, j - partBegin));
						i = j;
						break;
					}
				}
				else if (j == command.length() || isspace(command[j]))
				{
					parts.emplace_back(command.substr(partBegin, j - partBegin));
					i = j;
					break;
				}
			}
		}

		if (parts.size() == 0)
			return output = "";

		std::string commandName = parts[0];

		for (size_t i = 0; i < commandName.length(); ++i)
			commandName[i] = std::tolower(commandName[i]);

		std::vector<std::string> args(parts.begin() + 1, parts.begin() + parts.size());

		const Command * foundCommand = nullptr;

		for (const Command * command : _commands)
			if (command->getName() == commandName)
			{
				foundCommand = command;
				if (command->tryInvoke(args, output))
					return output;
			}

		for (const std::unique_ptr<CommandAlias> & alias : _aliasCommands)
			if (alias->getName() == commandName)
			{
				foundCommand = alias.get();
				if (alias->tryInvoke(args, output))
					return output;
			}

		if (foundCommand != nullptr)
		{
			return output;
		}

		return output = "Command '" + commandName + "' not found. Type 'help' for a list of available commands.";
	}

	void registerCommand(const Command & command)
	{
		auto iter = std::find(_commands.begin(), _commands.end(), &command);

		if (iter != _commands.end())
			return;

		_commands.emplace_back(&command);

		std::sort(_commands.begin(), _commands.end(),
			[](const Command * a, const Command * b) -> bool
		{
			return a->getName() < b->getName();
		});
	}

	void unregisterCommand(const Command & command)
	{
		auto iter = std::find(_commands.begin(), _commands.end(), &command);

		if (iter == _commands.end())
			return;

		_commands.erase(iter);
	}
}
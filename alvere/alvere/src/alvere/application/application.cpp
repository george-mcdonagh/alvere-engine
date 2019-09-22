#include <chrono>

#include <glfw/glfw3.h>

#include "alvere/application/application.hpp"
#include "alvere/utils/exceptions.hpp"
#include "alvere/utils/logging.hpp"


#include "alvere/graphics/buffers.hpp"
#include "alvere/graphics/camera.hpp"
#include "alvere/graphics/render_commands.hpp"
#include "alvere/graphics/shader_program.hpp"
#include "alvere/graphics/sprite_batcher.hpp"
#include "alvere/graphics/texture.hpp"
#include "graphics_api/opengl/opengl_vertex_array.hpp"

namespace alvere
{
	Application::Application()
		: m_window(Window::New()), m_targetFrameRate(60.0f), m_running(true)
	{
		m_window->SetEventCallback(ALV_EVENT_BIND_FUNC(Application::onEvent));
	}

	void Application::run()
	{
		render_commands::SetClearColour({0.1f, 0.1f, 0.1f, 1.0f});

		float timeStepNanoseconds = 1000000000.f / m_targetFrameRate;
		float timeStepSeconds = 1.0f / m_targetFrameRate;
		float deltaTime;
		float frameTimer = 1.0f;
		int framesThisSecond = 0;

		auto tickStartTime = std::chrono::high_resolution_clock::now();
		std::chrono::nanoseconds timeStep((int)timeStepNanoseconds);
		std::chrono::nanoseconds deltaTimeChrono;
		std::chrono::nanoseconds lag(0);

		while (m_running)
		{
			framesThisSecond++;

			deltaTimeChrono = std::chrono::high_resolution_clock::now() - tickStartTime;
			tickStartTime = std::chrono::high_resolution_clock::now();
			lag += deltaTimeChrono;
			deltaTime = deltaTimeChrono.count() / 1000000000.0f;

			if ((frameTimer -= deltaTime) < 0.0f)
			{
				LogInfo("FPS: %i\n", framesThisSecond);

				framesThisSecond = 0;
				frameTimer = 1.0f;
			}

			try
			{
				while (lag >= timeStep)
				{
					lag -= timeStep;

					m_window->PollEvents();

					update(timeStepSeconds);
				}

				render_commands::Clear();

				render();

				m_window->SwapBuffers();
			}
			catch(FatalErrorException e)
			{
				LogError("%s\n", e.what());
				m_running = false;
			}
			catch(Exception e)
			{
				LogError("Runtime exception: %s\n", e.what());
			}
		}

		m_window.release();

		glfwTerminate();
	}

	void Application::onEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowCloseEvent>(ALV_EVENT_BIND_FUNC(Application::onWindowClose));
	}

	bool Application::onWindowClose(WindowCloseEvent& e)
	{
		m_running = false;
		return true;
	}
}
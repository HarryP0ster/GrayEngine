#pragma once
#include <pch.h>
#include "Engine.h"

namespace GrEngine
{
	Engine* Engine::context = nullptr;

	Engine::Engine(const AppParameters& Properties)
	{
		if (context == nullptr)
		{
			context = this;
		}

		physEngine = new GrEngineBullet::BulletAPI();
		pWindow = std::unique_ptr<AppWindow>(AppWindow::Init(Properties));
	}

	Engine::~Engine()
	{

	}

	void Engine::Run()
	{
		while (!glfwWindowShouldClose(pWindow.get()->getWindow()))
		{
			if (!isPaused)
				pWindow->OnStep();
		}

		pWindow->~AppWindow();
	}

	void Engine::Stop()
	{
		physEngine->CleanUp();
		glfwSetWindowShouldClose(pWindow.get()->getWindow(), true);
	}

	bool Engine::LoadObject(UINT id, const char* mesh_path, std::vector<std::string> textures_vector)
	{
		Pause();

		bool res = GetRenderer()->loadModel(id, mesh_path, textures_vector);

		Unpause();
		return res;
	}

	bool Engine::LoadFromGMF(UINT id, const char* filepath)
	{
		return GetRenderer()->loadModel(id, filepath);
	}

	bool Engine::AssignTextures(std::vector<std::string> textures, Entity* target)
	{
		Pause();

		bool res = pWindow->getRenderer()->assignTextures(textures, target);

		Unpause();
		return res;
	}

	void Engine::clearScene()
	{
		physEngine->CleanUp();
		pWindow->getRenderer()->clearDrawables();
	}

	void Engine::TerminateLiraries()
	{
		glfwTerminate();
	}

	bool Engine::PokeIt()
	{
		Logger::Out("You've just poked an engine", OutputColor::Gray, OutputType::Log);
		return true;
	}

	bool Engine::WriteGMF(const char* filepath, const char* mesh_path, std::vector<std::string> textures_vector)
	{
		return Globals::writeGMF(filepath, mesh_path, textures_vector);
	}

	Entity* Engine::AddEntity()
	{
		return pWindow->getRenderer()->addEntity();
	}

	std::string Engine::getExecutablePath()
	{
		return Globals::getExecutablePath();
	}

	void Engine::Pause()
	{
		GetRenderer()->waitForRenderer();
		isPaused = true;
	}

	void Engine::Unpause()
	{
		isPaused = false;
	}

	void Engine::LoadSkybox(const char* East, const char* West, const char* Top, const char* Bottom, const char* North, const char* South)
	{
		Pause();

		pWindow->getRenderer()->createSkybox(East, West, Top, Bottom, North, South);

		Unpause();
	}

	void Engine::TogglePhysicsState(bool state)
	{
		physEngine->TogglePhysicsState(state);
	}

	UINT Engine::GetSelectedEntityID()
	{
		return GetRenderer()->GetSelectedEntity()->GetEntityID();
	}

	void Engine::LoadScene(const char* path)
	{
		auto start = std::chrono::steady_clock::now();

		Pause();
		TogglePhysicsState(false);
		physEngine->CleanUp();
		GetContext()->GetRenderer()->LoadScene(path);
		Unpause();

		auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		Logger::Out("Level %s loaded in %d ms", OutputColor::Gray, OutputType::Log, path, (int)time);
	}

	void Engine::SaveScene(const char* path)
	{
		GetRenderer()->SaveScene(path);
	}

	POINTFLOAT Engine::GetCursorPosition()
	{
		POINTFLOAT res;
		double xpos, ypos;
		glfwGetCursorPos(pWindow->getWindow(), &xpos, &ypos);
		res = { (float)xpos, (float)ypos };

		return res;
	}

	POINT Engine::GetWindowSize()
	{
		int w, h;
		glfwGetWindowSize(pWindow->getWindow(), &w, &h);
		return { w, h };
	}

	POINT Engine::GetWindowPosition()
	{
		int x, y;
		glfwGetWindowPos(pWindow->getWindow(), &x, &y);
		return { x, y };
	}

	void Engine::SetCursorShape(int shape)
	{
		GLFWcursor* cur = glfwCreateStandardCursor(shape);
		glfwSetCursor(pWindow->getWindow(), cur);
	}

	void Engine::SetCursorPosition(double xpos, double ypos)
	{
		glfwSetCursorPos(pWindow->getWindow(), xpos, ypos);
	}
}

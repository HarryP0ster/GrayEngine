#pragma once
#include <pch.h>
#include "Engine.h"

namespace GrEngine
{
	Engine::Engine(const AppParameters& Properties)
	{
		pWindow = std::unique_ptr<AppWindow>(AppWindow::Init(Properties));
	}

	Engine::~Engine()
	{

	}

	void Engine::Run()
	{
		while (!glfwWindowShouldClose(pWindow.get()->getWindow()))
		{
			pWindow->OnStep();
		}

		pWindow->~AppWindow();
	}

	void Engine::Stop()
	{
		glfwSetWindowShouldClose(pWindow.get()->getWindow(), true);
	}

	bool Engine::loadModel(const char* mesh_path, std::vector<std::string> textures_vector, std::unordered_map<std::string, std::string>* out_materials)
	{
		return pWindow->getRenderer()->loadModel(mesh_path, textures_vector, out_materials);
	}

	bool Engine::loadImageFromPath(const char* path, int material_index)
	{
		return pWindow->getRenderer()->loadImage(path, material_index);
	}

	void Engine::clearScene()
	{
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

	bool Engine::createModel(const char* filepath, const char* mesh_path, std::vector<std::string> textures_vector)
	{
		return pWindow->getRenderer()->writeGMF(filepath, mesh_path, textures_vector);
	}

	std::string Engine::getExecutablePath()
	{
		return Renderer::getExecutablePath();
	}
}

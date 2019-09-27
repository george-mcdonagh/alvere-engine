#include <alvere/alvere.hpp>
#include <alvere/assets.hpp>
#include <alvere/graphics/camera.hpp>
#include <alvere/graphics/mesh.hpp>
#include <alvere/graphics/renderer.hpp>
#include <alvere/graphics/sprite_batcher.hpp>
#include <alvere/graphics/texture.hpp>
#include <alvere/math/constants.hpp>
#include <alvere/math/matrix/transformations.hpp>
#include <alvere/utils/file_reader.hpp>
#include <alvere/world/scene.hpp>
#include <alvere/world/entity_component_systems/scene_renderer.hpp>
#include <alvere/world/entity_components/ec_rendered_mesh.hpp>

#include "tile_drawer.hpp"
#include "world_cell.hpp"
#include "world_cell_area.hpp"
#include "world_generation.hpp"

using namespace alvere;

struct AlvereApplication : public Application
{
	Asset<Shader> m_vertexShader;
	Asset<Shader> m_fragmentShader;
	Asset<ShaderProgram> m_shaderProgram;
	Asset<Texture> m_texture;
	Mesh * mesh;
	Material * material;
	MaterialInstance * m_materialInstance;

	Scene scene;
	Camera sceneCamera;
	Camera uiCamera;
	Asset<SpriteBatcher> m_spriteBatcher;
	Renderer * m_renderer;

	TileDrawer m_tileDrawer;
	WorldCellArea * m_worldCellArea;

	Font::Face * m_fontFace;

	AlvereApplication() : Application(),
		m_tileDrawer("res/img/tilesheet.png")
	{
		m_spriteBatcher = SpriteBatcher::New();

		world_generation::Generate(m_worldCellArea, 0);

		m_vertexShader = Shader::New(Shader::Type::Vertex, file::read("res/shaders/flat_colour.vert"));
		m_fragmentShader = Shader::New(Shader::Type::Fragment, file::read("res/shaders/flat_colour.frag"));
		m_shaderProgram = ShaderProgram::New();
		m_shaderProgram->SetShader(alvere::AssetRef<Shader>(m_vertexShader.get()));
		m_shaderProgram->SetShader(alvere::AssetRef<Shader>(m_fragmentShader.get()));
		m_shaderProgram->build();

		m_texture = Texture::New("res/img/falcon.jpg");

		mesh = new Mesh("res/meshes/teapot2.obj");
		
		material = new Material(m_shaderProgram.get());

		m_materialInstance = new MaterialInstance(material);
		m_materialInstance->get<Shader::DataType::Float3>("u_colour")->m_value = Vector3(0.8f, 0.1f, 0.3f);
		m_materialInstance->get<Shader::DataType::Sampler2D>("u_albedo")->m_value = m_texture.get();

		m_renderer = Renderer::New();

		sceneCamera.SetPosition(0, 0, 10);
		sceneCamera.SetPerspective(67.0f * _TAU_DIV_360, 1.0f, 0.01f, 1000.0f);

		uiCamera.SetOrthographic(0, 800, 800, 0, -1.0f, 1.0f);

		scene.addSystem<SceneRenderer>();

		Entity newEntity = scene.createEntity();
		scene.addEntityComponent<ECRenderedMesh>(newEntity);

		/*
		{
			Scene scene;
			scene.addSystem<RendererSystem>();
			scene.addSystem<TransformMoverSystem>();

			EntityHandle entity = scene.createEntity();
			entity.addComponent<Mesh>();

			EntityHandle cameraEntity = scene.createEntity();
			cameraEntity.addComponent<Camera>();
			cameraEntity.addComponent<Mesh>();
			cameraEntity.addComponent<TransformMover>();

			Camera::Handle camera = cameraEntity.getComponent<Camera>();

			cameraEntity.removeComponent<Mesh>();

			scene.destroyEntity(entity);

			...

			scene.update(deltaTime);

			scene.draw();



			entity1 = scene.createEntity();

			ECCamera * ec_camera = scene.createEntityComponent<ECCamera>(entity1);
			camera = &ec_camera->camera;
			camera->SetPosition(0, 0, 10);
			camera->SetPerspective(67.0f * _TAU_DIV_360, 1.0f, 0.01f, 1000.0f);

			ECRenderedMesh * ec_renderedMesh = scene.createEntityComponent<ECRenderedMesh>(entity1);
			ec_renderedMesh->mesh = mesh;
			ec_renderedMesh->material = m_materialInstance;

			entity1.get()->transform().setScale(Vector3(0.1f));
		}

		{
			entity2 = scene.createEntity();

			ECRenderedMesh * ec_renderedMesh = scene.createEntityComponent<ECRenderedMesh>(entity2);
			ec_renderedMesh->mesh = mesh;
			ec_renderedMesh->material = m_materialInstance;

			scene.setEntityParent(entity2, entity1);

			entity2.get()->transform().setPosition(Vector3(1.0f));
		}
		*/

		m_fontFace = new Font::Face("res/fonts/arial/arial.ttf");
	}

	~AlvereApplication()
	{
		delete m_worldCellArea;
		delete mesh;
		delete material;
		delete m_materialInstance;
		delete m_renderer;
		delete m_fontFace;
	}

	void update(float deltaTime) override
	{
		Vector3 velocity;
		Vector3 rotation;

		const float moveSpeed = 4.0f;
		const float turnSpeed = 2.0f;
		const float mouseDamper = 0.1f;

		if (m_window->GetKey(Key::W)) velocity += Camera::forward * moveSpeed;
		if (m_window->GetKey(Key::S)) velocity -= Camera::forward * moveSpeed;
		if (m_window->GetKey(Key::D)) velocity += Camera::right * moveSpeed;
		if (m_window->GetKey(Key::A)) velocity -= Camera::right * moveSpeed;
		if (m_window->GetKey(Key::Space)) velocity += Camera::up * moveSpeed;
		if (m_window->GetKey(Key::LeftShift)) velocity -= Camera::up * moveSpeed;

		velocity *= deltaTime;
		sceneCamera.Move(velocity * sceneCamera.GetRotation());

		if (m_window->GetMouse().GetButton(MouseButton::Left))
		{
			m_window->DisableCursor();

			rotation -= Camera::up * m_window->GetMouse().delta.x * mouseDamper;
			rotation -= Camera::right * m_window->GetMouse().delta.y * mouseDamper;
		}
		else
		{
			m_window->EnableCursor();

			if (m_window->GetKey(Key::Left)) rotation += Camera::up * turnSpeed;
			if (m_window->GetKey(Key::Right)) rotation -= Camera::up * turnSpeed;
			if (m_window->GetKey(Key::Up)) rotation += Camera::right * turnSpeed;
			if (m_window->GetKey(Key::Down)) rotation -= Camera::right * turnSpeed;
		}
		
		if (m_window->GetKey(Key::E)) rotation += Camera::forward * turnSpeed;
		if (m_window->GetKey(Key::Q)) rotation -= Camera::forward * turnSpeed;

		sceneCamera.Rotate(Quaternion::fromEulerAngles(rotation * deltaTime));

		scene.update(deltaTime);
	}

	void render() override
	{
		scene.draw();

		m_spriteBatcher->Begin(sceneCamera.GetProjectionViewMatrix());
		 
		for (unsigned int x = 0; x < m_worldCellArea->GetWidth(); x++)
		{
			for (unsigned int y = 0; y < m_worldCellArea->GetHeight(); y++)
			{
				WorldCell& cell = m_worldCellArea->At(x, y);
				m_tileDrawer.DrawTile(*m_spriteBatcher, cell.m_x, cell.m_y, cell.m_Type);
			}
		}

		m_spriteBatcher->end();

		m_spriteBatcher->Begin(uiCamera.GetProjectionViewMatrix());

		std::string cameraPositionString = "x: " + std::to_string(sceneCamera.GetPosition().x) + "\ny: " + std::to_string(sceneCamera.GetPosition().y) + "\nz: " + std::to_string(sceneCamera.GetPosition().z);

		m_spriteBatcher->submit(*m_fontFace->getBitmap(18), cameraPositionString, Vector2(0, 800 - 18), Vector4(1.0f, 1.0f, 1.0f, 1.0f));

		m_spriteBatcher->end();
	}
};

ALV_NEW_APP_FUNC(AlvereApplication)
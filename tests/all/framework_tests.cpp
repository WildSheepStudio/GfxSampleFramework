#include <frm/def.h>

#include <frm/interpolation.h>
#include <frm/gl.h>
#include <frm/AppSample3d.h>
#include <frm/Buffer.h>
#include <frm/Curve.h>
#include <frm/Framebuffer.h>
#include <frm/GlContext.h>
#include <frm/Input.h>
#include <frm/Mesh.h>
#include <frm/MeshData.h>
#include <frm/Profiler.h>
#include <frm/Property.h>
#include <frm/Shader.h>
#include <frm/SkeletonAnimation.h>
#include <frm/Spline.h>
#include <frm/Texture.h>
#include <frm/Window.h>
#include <frm/XForm.h>

#include <apt/ArgList.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <EASTL/vector.h>

using namespace frm;
using namespace apt;

class AppSampleTest: public AppSample3d
{
public:
	typedef AppSample3d AppBase;

	struct MeshTest {
		FileSystem::PathStr m_meshPath;
		Mesh*               m_mesh;
		FileSystem::PathStr m_animPath;
		SkeletonAnimation*  m_anim;
		float               m_animTime;
		float               m_animSpeed;
		eastl::vector<int>  m_animHints;
		Shader*             m_shMeshShaded;
		Shader*             m_shMeshLines;
		Buffer*             m_bfSkinning;
		mat4                m_worldMatrix;
	} m_meshTest;

	struct DepthTest {
		FileSystem::PathStr m_meshPath;
		Mesh*               m_mesh;
		int                 m_meshCount;
		Buffer*             m_bfInstances;
		Shader*             m_shDepthOnly;
		Shader*             m_shDepthError;
		float               m_maxError;
	} m_depthTest;

	frm::Texture* m_txRadar;

	frm::Texture* m_txTest;

	AppSampleTest()
		: AppBase("AppSampleTest") 
	{
		memset(&m_meshTest, 0, sizeof(MeshTest));
		PropertyGroup& meshTestProps = m_props.addGroup("MeshTest");
		//                      name                     default                                  min     max     storage
		meshTestProps.addPath  ("Mesh Path",             "models/md5/bob_lamp_update.md5mesh",                    &m_meshTest.m_meshPath);
		meshTestProps.addPath  ("Anim Path",             "models/md5/bob_lamp_update.md5anim",                    &m_meshTest.m_animPath);

		memset(&m_depthTest, 0, sizeof(DepthTest));
		PropertyGroup& depthTestProps = m_props.addGroup("DepthTest");
		//                      name                     default                                  min     max     storage
		depthTestProps.addPath ("Mesh Path",             "models/teapot.obj",                     &m_depthTest.m_meshPath);
		depthTestProps.addInt  ("Mesh Count",            64,                                      1,      128,    &m_depthTest.m_meshCount);
		depthTestProps.addFloat("Max Error",             0.0001f,                                 0.0f,   1.0f,   &m_depthTest.m_maxError);
	}
	
	virtual bool init(const apt::ArgList& _args) override
	{
		if (!AppBase::init(_args)) {
			return false;
		}

		m_txRadar = Texture::Create("textures/radar.tga");
		m_txRadar->setWrap(GL_CLAMP_TO_EDGE);

		return true;
	}

	virtual void shutdown() override
	{
		Buffer::Destroy(m_meshTest.m_bfSkinning);
		Mesh::Release(m_meshTest.m_mesh);
		SkeletonAnimation::Release(m_meshTest.m_anim);
		Shader::Release(m_meshTest.m_shMeshLines);
		Shader::Release(m_meshTest.m_shMeshShaded);

		Mesh::Release(m_depthTest.m_mesh);
		Buffer::Destroy(m_depthTest.m_bfInstances);
		Shader::Release(m_depthTest.m_shDepthOnly);
		Shader::Release(m_depthTest.m_shDepthError);

		Texture::Release(m_txRadar);

		AppBase::shutdown();
	}

	virtual bool update() override
	{
		if (!AppBase::update()) {
			return false;
		}
		
		ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode("Intersection")) {
			Im3d::PushDrawState();

			enum Primitive
			{
				Primitive_Sphere,
				Primitive_Plane,
				Primitive_AlignedBox,
				Primitive_Cylinder,
				Primitive_Capsule
			};
			const char* primitiveList =
				"Sphere\0"
				"Plane\0"
				"AlignedBox\0"
				"Cylinder\0"
				"Capsule\0"
				;
			static int currentPrim = Primitive_AlignedBox;
			ImGui::Combo("Primitive", &currentPrim, primitiveList);
			static int useLine = 1;
			ImGui::RadioButton("Ray", &useLine, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Line", &useLine, 1);

			static mat4 primMat = identity;
			static float length = 3.0f;
			static float width  = 3.0f;
			static float radius = 1.0f;

			Ray ray;
			ray.m_origin = Scene::GetCullCamera()->getPosition();
			ray.m_direction = Scene::GetCullCamera()->getViewVector();
			Line line(ray.m_origin, ray.m_direction);
			bool intersects = false;
			bool intersectCheck = false;
			bool inFrustum = false;
			float t0, t1;

			#define Intersect1(prim) \
				if (useLine) { \
					intersects = Intersect(line, prim, t0); \
					intersectCheck = Intersects(line, prim); \
				} else { \
					intersects = Intersect(ray, prim, t0); \
					intersectCheck = Intersects(ray, prim); \
				}
			#define Intersect2(prim) \
				if (useLine) { \
					intersects = Intersect(line, prim, t0, t1); \
					intersectCheck = Intersects(line, prim); \
				} else { \
					intersects = Intersect(ray, prim, t0, t1); \
					intersectCheck = Intersects(ray, prim); \
				}
	
			Im3d::Gizmo("Primitive", (float*)&primMat);
			Im3d::SetColor(Im3d::Color_Red);
			Im3d::SetSize(3.0f);
			switch ((Primitive)currentPrim) {
				case Primitive_Sphere: {
					ImGui::SliderFloat("Radius", &radius, 0.0f, 8.0f);
					Sphere sphere(vec3(0.0f), radius);
					sphere.transform(primMat);
					Intersect2(sphere);
					inFrustum = Scene::GetCullCamera()->m_worldFrustum.inside(sphere);
					Im3d::PushAlpha(inFrustum ? 1.0f : 0.1f);
						Im3d::DrawSphere(sphere.m_origin, sphere.m_radius);
					Im3d::PopAlpha();
					break;
				}
				case Primitive_Plane: {
					ImGui::SliderFloat("Display Size", &width, 0.0f, 8.0f);
					Plane plane(vec3(0.0f, 1.0f, 0.0f), 0.0f);
					plane.transform(primMat);
					Intersect1(plane);
					t1 = t0;
					Im3d::DrawQuad(plane.getOrigin(), plane.m_normal, vec2(width));
					Im3d::BeginLines();
						Im3d::Vertex(plane.getOrigin());
						Im3d::Vertex(plane.getOrigin() + plane.m_normal);
					Im3d::End();
					break;
				}
				case Primitive_AlignedBox: {
					ImGui::SliderFloat("X", &length, 0.0f, 8.0f);
					ImGui::SliderFloat("Y", &width,  0.0f, 8.0f);
					ImGui::SliderFloat("Z", &radius, 0.0f, 8.0f);
					AlignedBox alignedBox(vec3(-length, -width, -radius) * 0.5f, vec3(length, width, radius) * 0.5f);
					alignedBox.transform(primMat);
					Intersect2(alignedBox);
					inFrustum = Scene::GetCullCamera()->m_worldFrustum.inside(alignedBox);
					Im3d::PushAlpha(inFrustum ? 1.0f : 0.1f);
						Im3d::DrawAlignedBox(alignedBox.m_min, alignedBox.m_max);
					Im3d::PopAlpha();
					break;
				}
				case Primitive_Cylinder: {
					ImGui::SliderFloat("Length", &length, 0.0f, 8.0f);
					ImGui::SliderFloat("Radius", &radius, 0.0f, 8.0f);
					Cylinder cylinder(vec3(0.0f, -length * 0.5f, 0.0f), vec3(0.0f, length * 0.5f, 0.0f), radius);
					cylinder.transform(primMat);
					Intersect2(cylinder);
					Im3d::DrawCylinder(cylinder.m_start, cylinder.m_end, cylinder.m_radius);
					break;
				}
				case Primitive_Capsule:	{
					ImGui::SliderFloat("Length", &length, 0.0f, 8.0f);
					ImGui::SliderFloat("Radius", &radius, 0.0f, 8.0f);
					Capsule capsule(vec3(0.0f, -length * 0.5f, 0.0f), vec3(0.0f, length * 0.5f, 0.0f), radius);
					capsule.transform(primMat);
					Intersect2(capsule);
					Im3d::DrawCapsule(capsule.m_start, capsule.m_end, capsule.m_radius);
					break;
				}
				default:
					APT_ASSERT(false);
					break;
			};
			#undef Intersect1
			#undef Intersect2
			
			ImGui::Text("Intersects: %s", intersects ? "TRUE" : "FALSE");
			ImGui::SameLine();
			ImGui::TextColored((intersectCheck == intersects) ? ImColor(0.0f, 1.0f, 0.0f) : ImColor(1.0f, 0.0f, 0.0f), "+");
			Im3d::PushAlpha(0.7f);
			Im3d::BeginLines();
				if (useLine) {
					Im3d::Vertex(line.m_origin - line.m_direction * 999.0f, 1.0f, Im3d::Color_Cyan);
					Im3d::Vertex(line.m_origin + line.m_direction * 999.0f, 1.0f, Im3d::Color_Cyan);
				} else {
					Im3d::Vertex(ray.m_origin, 1.0f, Im3d::Color_Cyan);
					Im3d::Vertex(ray.m_origin + ray.m_direction * 999.0f, 1.0f, Im3d::Color_Cyan);
				}
			Im3d::End();
			Im3d::PopAlpha();
			if (intersects) {
				ImGui::TextColored(ImColor(0.0f, 0.0f, 1.0f), "t0 %.3f", t0);
				ImGui::SameLine();
				ImGui::TextColored(ImColor(0.0f, 1.0f, 0.0f), "t1 %.3f", t1);
				Im3d::BeginLines();
					Im3d::Vertex(ray.m_origin + ray.m_direction * t0, Im3d::Color_Blue);
					Im3d::Vertex(ray.m_origin + ray.m_direction * t1, Im3d::Color_Green);
				Im3d::End();
				Im3d::BeginPoints();
					Im3d::Vertex(ray.m_origin + ray.m_direction * t0, 8.0f, Im3d::Color_Blue);
					Im3d::Vertex(ray.m_origin + ray.m_direction * t1, 6.0f, Im3d::Color_Green);
				Im3d::End();			
			}
 
			Im3d::PopDrawState();

			if (currentPrim == Primitive_AlignedBox || currentPrim == Primitive_Sphere) {
				ImGui::Text("In Frustum: %s", inFrustum ? "TRUE" : "FALSE");
			}

			static bool enablePerf = false;	
			ImGui::Checkbox("Perf Test", &enablePerf);
			if (enablePerf) {
				static int opCount = 100000;
				ImGui::SliderInt("Op Count", &opCount, 1, 10000);
				double avg;
				#define PerfTest1(prim) \
					Timestamp t = Time::GetTimestamp(); \
					for (int i = 0; i < opCount; ++i) \
						Intersect(ray, prim, t0); \
					avg = (Time::GetTimestamp() - t).asMicroseconds();
				#define PerfTest2(prim) \
					Timestamp t = Time::GetTimestamp(); \
					for (int i = 0; i < opCount; ++i) \
						Intersect(ray, prim, t0, t1); \
					avg = (Time::GetTimestamp() - t).asMicroseconds();

				switch ((Primitive)currentPrim) {
					case Primitive_Sphere: {
						Sphere sphere(vec3(0.0f), radius);
						sphere.transform(primMat);
						PerfTest2(sphere);
						break;
					}
					case Primitive_Plane: {
						Plane plane(vec3(0.0f, 1.0f, 0.0f), 0.0f);
						plane.transform(primMat);
						PerfTest1(plane);
						break;
					}
					case Primitive_AlignedBox: {
						AlignedBox alignedBox(vec3(-length, -width, -radius) * 0.5f, vec3(length, width, radius) * 0.5f);
						alignedBox.transform(primMat);
						PerfTest2(alignedBox);
						break;
					}
					case Primitive_Cylinder: {
						Cylinder cylinder(vec3(0.0f, -length * 0.5f, 0.0f), vec3(0.0f, length * 0.5f, 0.0f), radius);
						cylinder.transform(primMat);
						PerfTest2(cylinder);
						break;
					}
					case Primitive_Capsule: {
						Capsule capsule(vec3(0.0f, -length * 0.5f, 0.0f), vec3(0.0f, length * 0.5f, 0.0f), radius);
						capsule.transform(primMat);
						PerfTest2(capsule);
						break;
					}
					default:
						APT_ASSERT(false);
						break;
				};
				#undef PerfTest1
				#undef PerfTest2
				avg /= (double)opCount;
				ImGui::Text("%fus", (float)avg);
			}

			ImGui::TreePop();
		}

		return true;
	}

	virtual void drawMainMenuBar() override
	{
		AppBase::drawMainMenuBar();
	}

	virtual void drawStatusBar() override
	{
		AppBase::drawStatusBar();
	}

	virtual void draw() override
	{
		GlContext* ctx = GlContext::GetCurrent();
		Camera* drawCam = Scene::GetDrawCamera();
		Camera* cullCam = Scene::GetCullCamera();

		//ImGui::SetNextTreeNodeOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode("Mesh/Anim")) {
			APT_ONCE {
				if (!m_meshTest.m_shMeshShaded) {
					m_meshTest.m_shMeshShaded = Shader::CreateVsFs("shaders/MeshView_vs.glsl", "shaders/MeshView_fs.glsl", "SKINNING\0SHADED\0");
				} 
				if (!m_meshTest.m_shMeshLines) {
					m_meshTest.m_shMeshLines = Shader::CreateVsGsFs("shaders/MeshView_vs.glsl", "shaders/MeshView_gs.glsl", "shaders/MeshView_fs.glsl", "SKINNING\0LINES\0");
				}

				if (!m_meshTest.m_meshPath.isEmpty()) {
					m_meshTest.m_mesh = Mesh::Create((const char*)m_meshTest.m_meshPath);
					Buffer::Destroy(m_meshTest.m_bfSkinning);
					if (m_meshTest.m_mesh && m_meshTest.m_mesh->getBindPose()) {
						m_meshTest.m_bfSkinning = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(mat4) * m_meshTest.m_mesh->getBindPose()->getBoneCount(), GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);
						m_meshTest.m_bfSkinning->setName("_bfSkinning");
						mat4* bf = (mat4*)m_meshTest.m_bfSkinning->map(GL_WRITE_ONLY);
						for (int i = 0; i < m_meshTest.m_mesh->getBindPose()->getBoneCount(); ++i) {
							bf[i] = mat4(1.0f);
						}
						m_meshTest.m_bfSkinning->unmap();
						
					}
					m_meshTest.m_worldMatrix = RotationMatrix(vec3(-1.0f, 0.0f, 0.0f), Radians(90.0f));
				}
				if (!m_meshTest.m_animPath.isEmpty()) {
					m_meshTest.m_anim = SkeletonAnimation::Create((const char*)m_meshTest.m_animPath);
					m_meshTest.m_animTime = 0.0f;
					m_meshTest.m_animSpeed = 1.0f;
					m_meshTest.m_animHints.resize(m_meshTest.m_anim->getTrackCount(), 0);
				}
			
			}

			Im3d::Gizmo("MeshTestWorldMatrix", (float*)&m_meshTest.m_worldMatrix);
			if (m_meshTest.m_anim && m_meshTest.m_mesh && m_meshTest.m_mesh->getBindPose()) {
				ImGui::SliderFloat("Anim Time", &m_meshTest.m_animTime, 0.0f, 1.0f);
				ImGui::SliderFloat("Anim Speed", &m_meshTest.m_animSpeed, 0.0f, 2.0f);
				m_meshTest.m_animTime = Fract(m_meshTest.m_animTime + (float)m_deltaTime * m_meshTest.m_animSpeed);
				Skeleton framePose = m_meshTest.m_anim->getBaseFrame();
				{	PROFILER_MARKER_CPU("Skinning");
					m_meshTest.m_anim->sample(m_meshTest.m_animTime, framePose, m_meshTest.m_animHints.data());
					framePose.resolve();
					mat4* bf = (mat4*)m_meshTest.m_bfSkinning->map(GL_WRITE_ONLY);
					for (int i = 0; i < framePose.getBoneCount(); ++i) {
						bf[i] = framePose.getPose()[i] * m_meshTest.m_mesh->getBindPose()->getPose()[i];
					}
					m_meshTest.m_bfSkinning->unmap();
					Im3d::PushMatrix(m_meshTest.m_worldMatrix);
						framePose.draw();
					Im3d::PopMatrix();
				}
			} else {
				
			}

			if (m_meshTest.m_mesh) {
				static bool showWireframe   = true;
				static bool showNormals     = false;
				static bool showTangents    = false;
				static bool showTexcoords   = false;
				static bool showBoneWeights = false;
				static float vectorLength   = 0.1f;
				ImGui::SliderFloat("Vector Length", &vectorLength, 0.0f, 1.0f);
				ImGui::Checkbox("Wireframe",  &showWireframe);
				ImGui::Checkbox("Normals", &showNormals);
				ImGui::Checkbox("Tangents", &showTangents);
				if (ImGui::Checkbox("Texcoords", &showTexcoords) && showBoneWeights) {
					showBoneWeights = false;
				}
				if (ImGui::Checkbox("Bone Weights",  &showBoneWeights) && showTexcoords) {
					showTexcoords = false;
				}

				ctx->setFramebufferAndViewport(0);
				glAssert(glClear(GL_DEPTH_BUFFER_BIT));

				static int submesh = 0;
				ImGui::SliderInt("Submesh", &submesh, 0, m_meshTest.m_mesh->getSubmeshCount() - 1);
				ctx->setMesh(m_meshTest.m_mesh, submesh);
				
				ctx->setShader(m_meshTest.m_shMeshShaded);
				ctx->setUniform("uWorldMatrix", m_meshTest.m_worldMatrix);
				ctx->setUniform("uViewMatrix",  drawCam->m_view);
				ctx->setUniform("uProjMatrix",  drawCam->m_proj);
				ctx->setUniform("uTexcoords",   (int)showTexcoords);
				ctx->setUniform("uBoneWeights", (int)showBoneWeights);
				ctx->bindBuffer(m_meshTest.m_bfSkinning);
				glAssert(glEnable(GL_DEPTH_TEST));
				glAssert(glEnable(GL_CULL_FACE));
				ctx->draw();
				glAssert(glDisable(GL_CULL_FACE));
				glAssert(glDisable(GL_DEPTH_TEST));

				ctx->setShader(m_meshTest.m_shMeshLines);
				ctx->setUniform("uWorldMatrix",  m_meshTest.m_worldMatrix);
				ctx->setUniform("uViewMatrix",   Scene::GetDrawCamera()->m_view);
				ctx->setUniform("uProjMatrix",   Scene::GetDrawCamera()->m_proj);
				ctx->setUniform("uVectorLength", vectorLength);
				ctx->setUniform("uWireframe",    (int)showWireframe);
				ctx->setUniform("uNormals",      (int)showNormals);
				ctx->setUniform("uTangents",     (int)showTangents);
				glAssert(glEnable(GL_DEPTH_TEST));
				glAssert(glDepthFunc(GL_LEQUAL));
				glAssert(glEnable(GL_BLEND));
				ctx->draw();
				glAssert(glDisable(GL_BLEND));
				glAssert(glDepthFunc(GL_LESS));
				glAssert(glDisable(GL_DEPTH_TEST));
			}
			
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Depth Test")) {
			PROFILER_MARKER("Depth Test");

			APT_ONCE {
				if (!m_depthTest.m_meshPath.isEmpty()) {
					m_depthTest.m_mesh = Mesh::Create((const char*)m_depthTest.m_meshPath);
				}
				m_depthTest.m_shDepthOnly = Shader::CreateVsFs("shaders/DepthTest/DepthTest_vs.glsl", "shaders/DepthTest/DepthTest_fs.glsl");
				m_depthTest.m_shDepthError = Shader::CreateVsFs("shaders/DepthTest/DepthTest_vs.glsl", "shaders/DepthTest/DepthTest_fs.glsl", "DEPTH_ERROR\0");

				m_depthTest.m_bfInstances = Buffer::Create(GL_SHADER_STORAGE_BUFFER, sizeof(mat4) * (256 * 256 + 1), GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);
			}

			enum DepthFormat {
				DepthFormat_16,
				DepthFormat_24,
				DepthFormat_32,
				DepthFormat_32F,

				DepthFormat_Count
			};
			static int depthFormat = (int)DepthFormat_24;
			static Texture* txDepth;
			static Texture* txColor;
			static Framebuffer* fbDepth;
			static Framebuffer* fbDepthColor;
			static bool reconstructPosition = false;

			bool reinitDepthTexture = false;
			APT_ONCE reinitDepthTexture = true;
			
			ImGui::SliderFloat("Max Error", &m_depthTest.m_maxError, 0.0f, 1.0f, "%0.3f", 2.0f);
			ImGui::Checkbox("Reconstruct Position", &reconstructPosition);
			reinitDepthTexture |= ImGui::Combo("Depth Format", &depthFormat,
				"DepthFormat_16\0"
				"DepthFormat_24\0"
				"DepthFormat_32\0"
				"DepthFormat_32F\0"
				);

			ImGui::Text("Proj Type: %s %s %s", 
				drawCam->getProjFlag(Camera::ProjFlag_Perspective) ? "PERSP " : "ORTHO ",
				drawCam->getProjFlag(Camera::ProjFlag_Infinite)    ? "INF "   : "",
				drawCam->getProjFlag(Camera::ProjFlag_Reversed)    ? "REV "   : ""
				);

			ImGui::SliderInt("Mesh Count", &m_depthTest.m_meshCount, 1, 128);
			static bool culling = true;
			ImGui::Checkbox("Culling", &culling);

			if (reinitDepthTexture) {
				Texture::Release(txDepth);
				Texture::Release(txColor);
				Framebuffer::Destroy(fbDepth);
				Framebuffer::Destroy(fbDepthColor);

				GLenum glDepthFormat = GL_NONE;
				switch (depthFormat) {
					case DepthFormat_16:  glDepthFormat = GL_DEPTH_COMPONENT16;  break;
					case DepthFormat_24:  glDepthFormat = GL_DEPTH_COMPONENT24;  break;
					case DepthFormat_32:  glDepthFormat = GL_DEPTH_COMPONENT32;  break;
					case DepthFormat_32F: glDepthFormat = GL_DEPTH_COMPONENT32F; break;
					default:              APT_ASSERT(false); break;
				};
				txDepth = Texture::Create2d(m_resolution.x, m_resolution.y, glDepthFormat);
				txDepth->setName("txDepth");
				fbDepth = Framebuffer::Create(txDepth);
				txColor = Texture::Create2d(m_resolution.x, m_resolution.y, GL_RGBA8);
				txColor->setName("txColor");
				fbDepthColor = Framebuffer::Create(2, txColor, txDepth);
			}

			int instCount = 0;
			{	PROFILER_MARKER_CPU("Instance Update");
				mat4* instanceData = (mat4*)m_depthTest.m_bfInstances->map(GL_WRITE_ONLY);
				const float radius = m_depthTest.m_mesh->getBoundingSphere().m_radius;
				const float spacing = radius * 2.0f;
				for (int x = 0; x < m_depthTest.m_meshCount; ++x) {
					float px = (float)(x - m_depthTest.m_meshCount / 2) * spacing;
					for (int z = 0; z < m_depthTest.m_meshCount; ++z) {
						float pz = (float)(z - m_depthTest.m_meshCount / 2) * spacing;
						vec3 p = vec3(px, 0.0f, pz);
						if (!culling || cullCam->m_worldFrustum.inside(Sphere(p, radius))) { 
							instanceData[instCount++] = TranslationMatrix(p);
						}
					}
				}
				instanceData[instCount++] = ScaleMatrix(vec3(1000.0f));
				m_depthTest.m_bfInstances->unmap();
			}
			ImGui::SameLine();
			ImGui::Text("(%d instances)", instCount);

			{	PROFILER_MARKER("Depth Only");
				glAssert(glDepthFunc(drawCam->getProjFlag(Camera::ProjFlag_Reversed) ? GL_GREATER : GL_LESS));
				glAssert(glClearDepth(drawCam->getProjFlag(Camera::ProjFlag_Reversed) ? 0.0f : 1.0f));
				ctx->setFramebufferAndViewport(fbDepth);
				glAssert(glClear(GL_DEPTH_BUFFER_BIT));
				ctx->setShader(m_depthTest.m_shDepthOnly);
				ctx->setMesh(m_depthTest.m_mesh);
				ctx->bindBuffer("_bfInstances", m_depthTest.m_bfInstances);
				ctx->bindBuffer("_bfCamera", drawCam->m_gpuBuffer);
				glAssert(glEnable(GL_DEPTH_TEST));
				glAssert(glEnable(GL_CULL_FACE));
				glAssert(glColorMask(0, 0, 0, 0));
				ctx->draw(instCount);
				glAssert(glColorMask(1, 1, 1, 1));
				glAssert(glDisable(GL_DEPTH_TEST));
				glAssert(glDisable(GL_CULL_FACE));
				glAssert(glDepthFunc(GL_LESS));
				glAssert(glClearDepth(1.0f));
			}


			{	PROFILER_MARKER("Depth Error");
				ctx->setFramebufferAndViewport(fbDepthColor);
				glAssert(glClear(GL_COLOR_BUFFER_BIT));
				ctx->setShader(m_depthTest.m_shDepthError);
				ctx->setMesh(m_depthTest.m_mesh);
				ctx->bindTexture("txDepth", txDepth);
				ctx->bindTexture("txRadar", m_txRadar);
				ctx->bindBuffer("_bfInstances", m_depthTest.m_bfInstances);
				ctx->bindBuffer("_bfCamera", drawCam->m_gpuBuffer);
				ctx->setUniform("uMaxError", m_depthTest.m_maxError);
				ctx->setUniform("uReconstructPosition", (int)reconstructPosition);
				glAssert(glDepthMask(0));
				glAssert(glEnable(GL_DEPTH_TEST));
				glAssert(glDepthFunc(GL_EQUAL));
				glAssert(glEnable(GL_CULL_FACE));
				ctx->draw(instCount);
				glAssert(glDisable(GL_CULL_FACE));
				glAssert(glDepthFunc(GL_LESS));
				glAssert(glDisable(GL_DEPTH_TEST));
				glAssert(glDepthMask(1));
			}

			ctx->blitFramebuffer(fbDepthColor, 0, GL_COLOR_BUFFER_BIT);

			ImGui::TreePop();
		}

		AppBase::draw();
	}
};
AppSampleTest s_app;

int main(int _argc, char** _argv)
{
	AppSample* app = AppSample::GetCurrent();
	if (!app->init(ArgList(_argc, _argv))) {
		APT_ASSERT(false);
		return 1;
	}
	Window* win = app->getWindow();
	GlContext* ctx = app->getGlContext();
	while (app->update()) {
		APT_VERIFY(GlContext::MakeCurrent(ctx));
		glAssert(glViewport(0, 0, win->getWidth(), win->getHeight()));
		glAssert(glClearColor(0.3f, 0.3f, 0.3f, 0.0f));
		glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

		app->draw();
	}
	app->shutdown();

	return 0;
}

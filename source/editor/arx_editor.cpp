#include "../source/engine_pch.hpp"

#include "../source/editor/arx_editor.hpp"
#include "../source/managers/arx_buffer_manager.hpp"

namespace arx {
    Editor::Editor(ArxDevice& device, GLFWwindow* window, TextureManager& textureManager)
    : arxDevice(device), window(window), textureManager(textureManager) {
        std::filesystem::path path = std::filesystem::current_path() / ".." / ".." / "imgui.ini";
        iniPath = path.string();
    }

    Editor::~Editor() {
        cleanup();
    }

    void Editor::init() {
        createImGuiDescriptorPool();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

        io.IniFilename = iniPath.c_str();

        if (std::filesystem::exists(iniPath)) {
            ImGui::LoadIniSettingsFromDisk(iniPath.c_str());
        }

        ImGui_ImplGlfw_InitForVulkan(window, true);

        QueueFamilyIndices queueFamilyIndices = arxDevice.findPhysicalQueueFamilies();
        
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = arxDevice.getInstance();
        init_info.PhysicalDevice = arxDevice.getPhysicalDevice();
        init_info.Device = arxDevice.device();
        init_info.QueueFamily = queueFamilyIndices.graphicsFamily;
        init_info.Queue = arxDevice.graphicsQueue();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = imguiPool;
        init_info.Subpass = 0;
        init_info.MinImageCount = 2;
        init_info.ImageCount = 2;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = checkVkResult;
        init_info.RenderPass = renderPass;
        ImGui_ImplVulkan_Init(&init_info);

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
    }

    void Editor::cleanup() {
        saveLayout();

        if (imguiPool != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            
            vkDestroyDescriptorPool(arxDevice.device(), imguiPool, nullptr);
            imguiPool = VK_NULL_HANDLE;
        }
    }

    void Editor::newFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        createDockSpace();
    }

    void Editor::render(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet) {
        drawConsoleWindow();
        
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        
        ImGui::Image((ImTextureID)descriptorSet, viewportPanelSize);
        
        ImGui::End();

        ImGui::Render();
        ImDrawData* main_draw_data = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(main_draw_data, commandBuffer);

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void Editor::drawConsoleWindow() {
        ImGui::Begin("Console");

        ImGui::Checkbox("Auto-scroll", &autoScroll);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            consoleMessages.clear();
        }

        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            for (const auto& message : consoleMessages) {
                ImGui::TextUnformatted(message.c_str());
            }
        }

        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }

    void Editor::addLogMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        consoleMessages.push_back(message);
        if (consoleMessages.size() > maxConsoleMessages) {
            consoleMessages.pop_front();
        }
    }

    void Editor::drawDebugWindow(EditorImGuiData& data) {
        ImGui::Begin("Debug");

        ImGui::Text("Press I for ImGui, O for game");
        ImGui::Text("Chunks %d", static_cast<uint32_t>(BufferManager::readDrawCommandCount()));
        ImGui::Checkbox("Frustum Culling", &data.frustumCulling);
        ImGui::Checkbox("Occlusion Culling", &data.occlusionCulling);
        ImGui::Checkbox("Freeze Culling", &data.enableCulling);
        ImGui::Checkbox("SSAO Enabled", &data.ssaoEnabled);
        ImGui::Checkbox("SSAO Only", &data.ssaoOnly);
        ImGui::Checkbox("SSAO Blur", &data.ssaoBlur);
        ImGui::Checkbox("Deferred", &data.deferred);

        if (!data.ssaoEnabled) data.ssaoOnly = false;
        if (data.ssaoOnly || !data.ssaoEnabled) data.ssaoBlur = false;
        if (data.ssaoOnly) data.deferred = false;

        ImGui::Text("Culling Time: %.3f ms", data.cullingTime / 1e6);
        ImGui::Text("Render Time: %.3f ms", data.renderTime / 1e6);
        ImGui::Text("Depth Pyramid Time: %.3f ms", data.depthPyramidTime / 1e6);
        ImGui::End();
    }

    void Editor::drawCoordinateVectors(ArxCamera& camera) {
        float dpiScale = ImGui::GetIO().DisplayFramebufferScale.x;

        // Get the current viewport
        ImGuiViewport* viewport = ImGui::GetWindowViewport();
        ImVec2 viewportPos = viewport->Pos;
        ImVec2 viewportSize = viewport->Size;

        // Calculate the center of the viewport
        ImVec2 viewportCenter = ImVec2((viewportPos.x + viewportSize.x * 0.5f) / dpiScale,
                                       (viewportPos.y + viewportSize.y * 0.5f) / dpiScale);

        // Set up a transparent window for drawing
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0)); // Transparent background
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));   // No border

        ImGui::SetNextWindowPos(ImVec2(viewportCenter.x - 50.0f * dpiScale,
                                       viewportCenter.y - 50.0f * dpiScale));
        ImGui::SetNextWindowSize(ImVec2(100 * dpiScale, 100 * dpiScale));

        // Begin a transparent window for drawing the coordinate vectors
        ImGui::Begin("Coordinate Vectors", nullptr, ImGuiWindowFlags_NoTitleBar |
                                                    ImGuiWindowFlags_NoResize |
                                                    ImGuiWindowFlags_NoMove |
                                                    ImGuiWindowFlags_NoScrollbar |
                                                    ImGuiWindowFlags_NoCollapse);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImVec2 origin = ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetWindowContentRegionMax().x * 0.5f, 
                               ImGui::GetCursorScreenPos().y + ImGui::GetWindowContentRegionMax().y * 0.5f);

        float length = 20.0f * dpiScale;

        ImU32 color_x = IM_COL32(255, 0, 0, 255);
        ImU32 color_y = IM_COL32(0, 255, 0, 255);
        ImU32 color_z = IM_COL32(0, 0, 255, 255);

        glm::mat3 viewRotation = glm::mat3(camera.getView());

        glm::vec3 xAxisWorld(1.0f, 0.0f, 0.0f);
        glm::vec3 yAxisWorld(0.0f, 1.0f, 0.0f);
        glm::vec3 zAxisWorld(0.0f, 0.0f, 1.0f);

        glm::vec3 xAxisView = viewRotation * xAxisWorld;
        glm::vec3 yAxisView = viewRotation * yAxisWorld;
        glm::vec3 zAxisView = viewRotation * zAxisWorld;
        
        // Project the transformed axes onto the 2D screen
        auto projectAxis = [&](const glm::vec3& axis) -> ImVec2 {
            return ImVec2(
                origin.x + axis.x * length,
                origin.y + axis.y * length
            );
        };

        ImVec2 xEnd = projectAxis(xAxisView);
        ImVec2 yEnd = projectAxis(yAxisView);
        ImVec2 zEnd = projectAxis(zAxisView);

        draw_list->AddLine(origin, xEnd, color_x, 2.0f);
        draw_list->AddText(xEnd, color_x, "X");

        draw_list->AddLine(origin, yEnd, color_y, 2.0f);
        draw_list->AddText(yEnd, color_y, "Y");

        draw_list->AddLine(origin, zEnd, color_z, 2.0f);
        draw_list->AddText(zEnd, color_z, "Z");

        ImGui::End();

        // Pop style colors to revert back to original
        ImGui::PopStyleColor(2);
    }

    void Editor::createImGuiDescriptorPool() {
        std::array<VkDescriptorPoolSize, 11> pool_sizes = {{
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        }};

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();

        if (vkCreateDescriptorPool(arxDevice.device(), &pool_info, nullptr, &imguiPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool for imgui!");
        }
    }

    void Editor::createDockSpace() {
        static bool dockspaceOpen = true;
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        } else {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        ImGui::End();
    }

    void Editor::checkVkResult(VkResult err) {
        if (err == 0) return;
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0) abort();
    }

    void Editor::destroyOldRenderPass() {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(arxDevice.device(), renderPass, nullptr);
        }
    }

    void Editor::saveLayout() {
        ImGui::SaveIniSettingsToDisk(iniPath.c_str());
    }
}

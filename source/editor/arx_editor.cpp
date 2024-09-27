#include "../source/engine_pch.hpp"

#include "../source/editor/arx_editor.hpp"
#include "../source/managers/arx_buffer_manager.hpp"

namespace arx {
    std::shared_ptr<ArxBuffer>  Editor::editorDataBuffer;
    Editor::EditorImGuiData     Editor::data;

    Editor::Editor(ArxDevice& device, GLFWwindow* window, TextureManager& textureManager)
    : arxDevice(device), window(window), textureManager(textureManager) {
        std::filesystem::path path = std::filesystem::current_path() / ".." / ".." / "imgui.ini";
        iniPath = path.string();

        editorDataBuffer = std::make_shared<ArxBuffer>(arxDevice,
                                                       sizeof(EditorImGuiData),
                                                       1,
                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    Editor::~Editor() {
        cleanup();
        editorDataBuffer.reset();
    }

    void Editor::init() {
        createImGuiDescriptorPool();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard 
                        | ImGuiConfigFlags_NavEnableGamepad 
                        | ImGuiConfigFlags_DockingEnable 
                        | ImGuiConfigFlags_ViewportsEnable;

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

        // Set up the dark theme
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowMinSize        = ImVec2(160, 20);
        style.FramePadding         = ImVec2(4, 2);
        style.ItemSpacing          = ImVec2(6, 2);
        style.ItemInnerSpacing     = ImVec2(6, 4);
        style.Alpha                = 1.0f;
        style.WindowRounding       = 0.0f;
        style.FrameRounding        = 0.0f;
        style.IndentSpacing        = 6.0f;
        style.ColumnsMinSpacing    = 50.0f;
        style.GrabMinSize          = 14.0f;
        style.GrabRounding         = 0.0f;
        style.ScrollbarSize        = 12.0f;
        style.ScrollbarRounding    = 0.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text]                  = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        colors[ImGuiCol_TextDisabled]          = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_WindowBg]              = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_ChildBg]               = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]               = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgActive]         = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_TitleBg]               = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_TitleBgActive]         = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_MenuBarBg]             = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.23f, 0.23f, 0.23f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_CheckMark]             = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        colors[ImGuiCol_SliderGrab]            = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_Button]                = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_ButtonHovered]         = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_ButtonActive]          = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_Header]                = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_HeaderHovered]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_HeaderActive]          = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_SeparatorActive]       = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_ResizeGrip]            = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_Tab]                   = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_TabHovered]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TabActive]             = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_TabUnfocused]          = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_PlotLines]             = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        colors[ImGuiCol_PlotHistogram]         = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_DragDropTarget]        = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        colors[ImGuiCol_NavHighlight]          = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
        colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

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
        drawProfilerWindow();
        
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

    void Editor::drawDebugWindow() {
        ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));

        // General info
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Debug Information");
        ImGui::Separator();
        ImGui::Text("Press I for ImGui, O for game");
        ImGui::Text("Chunks: %d", static_cast<uint32_t>(BufferManager::readDrawCommandCount()));
        ImGui::Spacing();

        // Camera parameters
        if (ImGui::CollapsingHeader("Camera Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Checkbox("Frustum Culling", reinterpret_cast<bool*>(&data.camera.frustumCulling));
            ImGui::Checkbox("Occlusion Culling", reinterpret_cast<bool*>(&data.camera.occlusionCulling));
            ImGui::Checkbox("Freeze Culling", reinterpret_cast<bool*>(&data.camera.disableCulling));
            ImGui::SliderFloat("zNear", &data.camera.zNear, 0.1f, 1000.0f);
            ImGui::SliderFloat("zFar", &data.camera.zFar, 100.0f, 2000.0f);
            ImGui::SliderFloat("Speed", &data.camera.speed, 1.0f, 320.0f);
            ImGui::Unindent();
        }

        // Lighting parameters
        if (ImGui::CollapsingHeader("Lighting Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            ImGui::Checkbox("SSAO Enabled", reinterpret_cast<bool*>(&data.lighting.ssaoEnabled));
            ImGui::BeginDisabled(data.lighting.ssaoEnabled == 0);
            ImGui::Checkbox("SSAO Only", reinterpret_cast<bool*>(&data.lighting.ssaoOnly));
            ImGui::BeginDisabled(data.lighting.ssaoOnly != 0);
            ImGui::Checkbox("SSAO Blur", reinterpret_cast<bool*>(&data.lighting.ssaoBlur));
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::BeginDisabled(data.lighting.ssaoOnly != 0);
            ImGui::Checkbox("Deferred", reinterpret_cast<bool*>(&data.lighting.deferred));
            ImGui::EndDisabled();
            ImGui::SliderFloat("Sun Kelvin", &data.lighting.directLightColor, 0.0f, 10000.0f);
            ImGui::SliderFloat("Sun Intensity", &data.lighting.directLightIntensity, 0.0f, 1.0f);
            ImGui::SliderFloat("Per Light Max Distance", &data.lighting.perLightMaxDistance, 0.1f, 20.0f);
            ImGui::Unindent();
        }

        ImGui::PopStyleVar();

        ImGui::End();
    }

    void Editor::drawProfilerWindow() {
        ImGui::Begin("Profiler");

        // Retrieve profiling data
        const auto& currentFrameData = Profiler::getProfileData(1); // Offset always 1 to fetch previous' frame timestamps
        profilerDataHistory.push_back(currentFrameData);

        // Maintain history size for averaging
        if (profilerDataHistory.size() > historySize) {
            profilerDataHistory.pop_front();
        }

        // Initialize average durations
        std::vector<std::pair<std::string, double>> averageDurations;
        std::unordered_map<std::string, size_t> stageIndexMap;

        for (const auto& frameData : profilerDataHistory) {
            for (size_t i = 0; i < frameData.size(); ++i) {
                const auto& [stage, duration] = frameData[i];
                if (stageIndexMap.find(stage) == stageIndexMap.end()) {
                    stageIndexMap[stage] = averageDurations.size();
                    averageDurations.emplace_back(stage, 0.0);
                }
                averageDurations[stageIndexMap[stage]].second += duration;
            }
        }

        // Calculate the average
        for (auto& [stage, totalDuration] : averageDurations) {
            totalDuration /= historySize;
        }

        if (averageDurations.empty()) {
            ImGui::Text("No profiling data available.");
            ImGui::End();
            return;
        }

        // Calculate total frame time by summing all average durations
        float totalTime = 0.0f;
        for (const auto& [stage, duration] : averageDurations) {
            totalTime += static_cast<float>(duration);
        }

        const float bar_height = 10.0f;
        const float spacing = 2.0f;
        const float text_width = 150.0f;
        const float duration_text_width = 70.0f;
        const float bar_start_x = text_width + spacing;
        const float bar_width = ImGui::GetContentRegionAvail().x - bar_start_x - duration_text_width - spacing;

        float startTime = 0.0f;

        size_t i = 0;
        for (const auto& [stageName, duration] : averageDurations) {

            ImVec4 stageColor = getColorForIndex(i++, averageDurations.size());
            ImGui::PushStyleColor(ImGuiCol_Text, stageColor);

            // Display stage name
            ImGui::Text("%s", stageName.c_str());
            ImGui::SameLine(bar_start_x);

            // Draw stage bar
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

            draw_list->AddRectFilled(
                ImVec2(cursor_pos.x + bar_width * (startTime / totalTime), cursor_pos.y),
                ImVec2(cursor_pos.x + bar_width * ((startTime + duration) / totalTime), cursor_pos.y + bar_height),
                ImColor(stageColor)
            );

            ImGui::Dummy(ImVec2(bar_width, bar_height));  // Reserve space for the bar
            ImGui::SameLine();

            // Display duration text
            char duration_text[32];
            snprintf(duration_text, sizeof(duration_text), "%.2f ms", static_cast<float>(duration));
            ImGui::Text("%s", duration_text);
            ImGui::PopStyleColor();

            // Distribute everything evenly inside the window
            ImGui::Spacing();

            // Update start time for the next stage
            startTime += static_cast<float>(duration);
        }

        // Display total frame time
        ImGui::Text("Total Time: %.2f ms", totalTime);
        ImGui::End();
    }

    ImVec4 Editor::getColorForIndex(int index, int total) {
        // Generate a unique color for each bar
        float hue = index / float(total);
        return ImColor::HSV(hue, 0.7f, 0.9f);
    }

    void Editor::drawCoordinateVectors(ArxCamera& camera) {
        float dpiScale = ImGui::GetIO().DisplayFramebufferScale.x;

        ImGui::Begin("Viewport");

        ImVec2 viewportPos = ImGui::GetWindowPos();
        ImVec2 viewportSize = ImGui::GetWindowSize();

        ImVec2 viewportCenter = ImVec2(viewportPos.x + viewportSize.x * 0.5f, 
                                       viewportPos.y + viewportSize.y * 0.5f);

        ImGui::End();

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

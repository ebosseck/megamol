/*
 * GUIManager.cpp
 *
 * Copyright (C) 2018 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */


#include "GUIManager.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h"
#include "mmcore/versioninfo.h"
#include "widgets/ButtonWidgets.h"
#include "widgets/CorporateGreyStyle.h"
#include "widgets/CorporateWhiteStyle.h"
#include "widgets/DefaultStyle.h"


using namespace megamol;
using namespace megamol::gui;


GUIManager::GUIManager()
        : hotkeys()
        , context(nullptr)
        , initialized_api(megamol::gui::GUIImGuiAPI::NONE)
        , state()
        , win_collection()
        , popup_collection()
        , notification_collection()
        , configurator_ptr(nullptr)
        , tfeditor_ptr(nullptr)
        , file_browser()
        , search_widget()
        , tooltip()
        , picking_buffer() {

    this->hotkeys[GUIManager::GuiHotkeyIndex::TRIGGER_SCREENSHOT] = {
        megamol::core::view::KeyCode(megamol::core::view::Key::KEY_F2, core::view::Modifier::NONE), false};
    this->hotkeys[GUIManager::GuiHotkeyIndex::TOGGLE_GRAPH_ENTRY] = {
        megamol::core::view::KeyCode(megamol::core::view::Key::KEY_F3, core::view::Modifier::NONE), false};
    this->hotkeys[GUIManager::GuiHotkeyIndex::EXIT_PROGRAM] = {
        megamol::core::view::KeyCode(megamol::core::view::Key::KEY_F4, core::view::Modifier::ALT), false};
    this->hotkeys[GUIManager::GuiHotkeyIndex::MENU] = {
        megamol::core::view::KeyCode(megamol::core::view::Key::KEY_F12, core::view::Modifier::NONE), false};
    this->hotkeys[GUIManager::GuiHotkeyIndex::PARAMETER_SEARCH] = {
        megamol::core::view::KeyCode(megamol::core::view::Key::KEY_P, core::view::Modifier::CTRL), false};
    this->hotkeys[GUIManager::GuiHotkeyIndex::SAVE_PROJECT] = {
        megamol::core::view::KeyCode(megamol::core::view::Key::KEY_S, core::view::Modifier::CTRL), false};
    this->hotkeys[GUIManager::GuiHotkeyIndex::LOAD_PROJECT] = {
        megamol::core::view::KeyCode(megamol::core::view::Key::KEY_L, core::view::Modifier::CTRL), false};
    this->hotkeys[GUIManager::GuiHotkeyIndex::SHOW_HIDE_GUI] = {
        megamol::core::view::KeyCode(megamol::core::view::Key::KEY_G, core::view::Modifier::CTRL), false};

    this->init_state();
}


GUIManager::~GUIManager() {

    this->destroy_context();
}


void megamol::gui::GUIManager::init_state() {

    this->state.gui_visible = true;
    this->state.gui_visible_post = true;
    this->state.gui_restore_hidden_windows.clear();
    this->state.gui_hide_next_frame = 0;
    this->state.style = GUIManager::Styles::DarkColors;
    this->state.rescale_windows = false;
    this->state.style_changed = true;
    this->state.new_gui_state = "";
    this->state.project_script_paths.clear();
    this->state.font_utf8_ranges.clear();
    this->state.load_fonts = false;
    this->state.win_delete_hash_id = 0;
    this->state.last_instance_time = 0.0;
    this->state.open_popup_about = false;
    this->state.open_popup_save = false;
    this->state.open_popup_load = false;
    this->state.open_popup_screenshot = false;
    this->state.menu_visible = true;
    this->state.graph_fonts_reserved = 0;
    this->state.toggle_graph_entry = false;
    this->state.shutdown_triggered = false;
    this->state.screenshot_triggered = false;
    this->state.screenshot_filepath = "megamol_screenshot.png";
    this->state.screenshot_filepath_id = 0;
    this->state.hotkeys_check_once = true;
    this->state.font_apply = false;
    this->state.font_file_name = "";
    this->state.request_load_projet_file = "";
    this->state.stat_averaged_fps = 0.0f;
    this->state.stat_averaged_ms = 0.0f;
    this->state.stat_frame_count = 0;
    this->state.font_size = 13;
    this->state.load_docking_preset = true;

    this->create_not_existing_png_filepath(this->state.screenshot_filepath);
}


bool GUIManager::CreateContext(GUIImGuiAPI imgui_api) {

    // Check prerequisities for requested API
    switch (imgui_api) {
    case (GUIImGuiAPI::OPEN_GL): {
        bool prerequisities_given = true;
#ifdef _WIN32 // Windows
        HDC ogl_current_display = ::wglGetCurrentDC();
        HGLRC ogl_current_context = ::wglGetCurrentContext();
        if (ogl_current_display == nullptr) {
            megamol::core::utility::log::Log::DefaultLog.WriteMsg(
                megamol::core::utility::log::Log::LEVEL_ERROR, "[GUI] There is no OpenGL rendering context available.");
            prerequisities_given = false;
        }
        if (ogl_current_context == nullptr) {
            megamol::core::utility::log::Log::DefaultLog.WriteMsg(megamol::core::utility::log::Log::LEVEL_ERROR,
                "[GUI] There is no current OpenGL rendering context available from the calling thread.");
            prerequisities_given = false;
        }
#else
        /// XXX The following throws segfault if OpenGL is not loaded yet:
        // Display* gl_current_display = ::glXGetCurrentDisplay();
        // GLXContext ogl_current_context = ::glXGetCurrentContext();
        /// XXX Is there a better way to check existing OpenGL context?
        if (glXGetCurrentDisplay == nullptr) {
            megamol::core::utility::log::Log::DefaultLog.WriteMsg(
                megamol::core::utility::log::Log::LEVEL_ERROR, "[GUI] There is no OpenGL rendering context available.");
            prerequisities_given = false;
        }
        if (glXGetCurrentContext == nullptr) {
            megamol::core::utility::log::Log::DefaultLog.WriteMsg(megamol::core::utility::log::Log::LEVEL_ERROR,
                "[GUI] There is no current OpenGL rendering context available from the calling thread.");
            prerequisities_given = false;
        }
#endif // _WIN32
        if (!prerequisities_given) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "[GUI] Failed to create ImGui context for OpenGL API. [%s, %s, line %d]\n<<< HINT: Check if "
                "project contains view module. >>>",
                __FILE__, __FUNCTION__, __LINE__);
            return false;
        }
    } break;
    default: {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] ImGui API is not supported. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    } break;
    }

    // Create ImGui Context
    bool other_context_exists = (ImGui::GetCurrentContext() != nullptr);
    if (this->create_context()) {

        // Initialize ImGui API
        if (!other_context_exists) {
            switch (imgui_api) {
            case (GUIImGuiAPI::OPEN_GL): {
                // Init OpenGL for ImGui
                if (ImGui_ImplOpenGL3_Init(nullptr)) {
                    megamol::core::utility::log::Log::DefaultLog.WriteInfo("[GUI] Created ImGui context for Open GL.");
                } else {
                    this->destroy_context();
                    megamol::core::utility::log::Log::DefaultLog.WriteError(
                        "[GUI] Unable to initialize OpenGL for ImGui. [%s, %s, line %d]\n", __FILE__, __FUNCTION__,
                        __LINE__);
                    return false;
                }

            } break;
            default: {
                this->destroy_context();
                megamol::core::utility::log::Log::DefaultLog.WriteError(
                    "[GUI] ImGui API is not supported. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
                return false;
            } break;
            }
        }

        this->initialized_api = imgui_api;
        megamol::gui::gui_context_count++;
        ImGui::SetCurrentContext(this->context);
        return true;
    }
    return false;
}


bool GUIManager::PreDraw(glm::vec2 framebuffer_size, glm::vec2 window_size, double instance_time) {

    // Handle multiple ImGui contexts.
    bool valid_imgui_scope =
        ((ImGui::GetCurrentContext() != nullptr) ? (ImGui::GetCurrentContext()->WithinFrameScope) : (false));
    if (this->state.gui_visible && valid_imgui_scope) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] Nesting ImGui contexts is not supported. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        this->state.gui_visible = false;
    }

    // (Delayed font loading for being resource directories available via resource in frontend)
    if (this->state.load_fonts) {
        this->load_default_fonts();
        this->state.load_fonts = false;
    }

    // Process hotkeys
    this->check_multiple_hotkey_assignment();
    if (this->hotkeys[GUIManager::GuiHotkeyIndex::SHOW_HIDE_GUI].is_pressed) {
        if (this->state.gui_visible) {
            this->state.gui_hide_next_frame = 2;
        } else { /// !this->state.gui_visible
            // Show GUI after it was hidden (before early exit!)
            // Restore window 'open' state (Always restore at least menu)
            this->state.menu_visible = true;
            const auto func = [&, this](WindowConfiguration& wc) {
                if (std::find(this->state.gui_restore_hidden_windows.begin(),
                        this->state.gui_restore_hidden_windows.end(),
                        wc.Name()) != this->state.gui_restore_hidden_windows.end()) {
                    wc.Config().show = true;
                }
            };
            this->win_collection.EnumWindows(func);
            this->state.gui_restore_hidden_windows.clear();
            this->state.gui_visible = true;
        }
        this->hotkeys[GUIManager::GuiHotkeyIndex::SHOW_HIDE_GUI].is_pressed = false;
    }
    if (this->hotkeys[GUIManager::GuiHotkeyIndex::EXIT_PROGRAM].is_pressed) {
        this->state.shutdown_triggered = true;
        return true;
    }
    if (this->hotkeys[GUIManager::GuiHotkeyIndex::TRIGGER_SCREENSHOT].is_pressed) {
        this->state.screenshot_triggered = true;
        this->hotkeys[GUIManager::GuiHotkeyIndex::TRIGGER_SCREENSHOT].is_pressed = false;
    }
    if (this->hotkeys[GUIManager::GuiHotkeyIndex::MENU].is_pressed) {
        this->state.menu_visible = !this->state.menu_visible;
        this->hotkeys[GUIManager::GuiHotkeyIndex::MENU].is_pressed = false;
    }
    if (this->state.toggle_graph_entry || this->hotkeys[GUIManager::GuiHotkeyIndex::TOGGLE_GRAPH_ENTRY].is_pressed) {
        if (auto graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph()) {
            auto module_graph_entry_iter = graph_ptr->Modules().begin();
            // Search for first graph entry and set next view to graph entry (= graph entry point)
            for (auto module_iter = graph_ptr->Modules().begin(); module_iter != graph_ptr->Modules().end();
                 module_iter++) {
                if ((*module_iter)->IsView() && (*module_iter)->IsGraphEntry()) {
                    // Remove all graph entries
                    (*module_iter)->SetGraphEntryName("");
                    Graph::QueueData queue_data;
                    queue_data.name_id = (*module_iter)->FullName();
                    graph_ptr->PushSyncQueue(Graph::QueueAction::REMOVE_GRAPH_ENTRY, queue_data);
                    // Save index of last found graph entry
                    if (module_iter != graph_ptr->Modules().end()) {
                        module_graph_entry_iter = module_iter + 1;
                    }
                }
            }
            if ((module_graph_entry_iter == graph_ptr->Modules().begin()) ||
                (module_graph_entry_iter != graph_ptr->Modules().end())) {
                // Search for next graph entry
                for (auto module_iter = module_graph_entry_iter; module_iter != graph_ptr->Modules().end();
                     module_iter++) {
                    if ((*module_iter)->IsView()) {
                        (*module_iter)->SetGraphEntryName(graph_ptr->GenerateUniqueGraphEntryName());
                        Graph::QueueData queue_data;
                        queue_data.name_id = (*module_iter)->FullName();
                        graph_ptr->PushSyncQueue(Graph::QueueAction::CREATE_GRAPH_ENTRY, queue_data);
                        break;
                    }
                }
            }
        }
        this->state.toggle_graph_entry = false;
        this->hotkeys[GUIManager::GuiHotkeyIndex::TOGGLE_GRAPH_ENTRY].is_pressed = false;
    }

    // Check for initialized imgui api
    if (this->initialized_api == GUIImGuiAPI::NONE) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] No ImGui API initialized. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    // Check for existing imgui context
    if (this->context == nullptr) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] No valid ImGui context available. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    // Set ImGui context
    ImGui::SetCurrentContext(this->context);

    // Required to prevent change in gui drawing between pre and post draw
    this->state.gui_visible_post = this->state.gui_visible;
    // Early exit when pre step should be omitted
    if (!this->state.gui_visible) {
        return true;
    }

    // Set stuff for next frame --------------------------------------------
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2(window_size.x, window_size.y);
    if ((window_size.x > 0.0f) && (window_size.y > 0.0f)) {
        io.DisplayFramebufferScale = ImVec2(framebuffer_size.x / window_size.x, framebuffer_size.y / window_size.y);
    }

    if ((instance_time - this->state.last_instance_time) < 0.0) {
        megamol::core::utility::log::Log::DefaultLog.WriteWarn(
            "[GUI] Current instance time results in negative time delta. [%s, %s, line %d]\n", __FILE__, __FUNCTION__,
            __LINE__);
    }
    io.DeltaTime = ((instance_time - this->state.last_instance_time) > 0.0)
                       ? (static_cast<float>(instance_time - this->state.last_instance_time))
                       : (io.DeltaTime);
    this->state.last_instance_time = ((instance_time - this->state.last_instance_time) > 0.0)
                                         ? (instance_time)
                                         : (this->state.last_instance_time + io.DeltaTime);

    // Style
    if (this->state.style_changed) {
        ImGuiStyle& style = ImGui::GetStyle();
        switch (this->state.style) {
        case (GUIManager::Styles::DarkColors): {
            DefaultStyle();
            ImGui::StyleColorsDark();
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_ChildBg] = style.Colors[ImGuiCol_WindowBg];
        } break;
        case (GUIManager::Styles::LightColors): {
            DefaultStyle();
            ImGui::StyleColorsLight();
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_ChildBg] = style.Colors[ImGuiCol_WindowBg];
        } break;
        case (GUIManager::Styles::CorporateGray): {
            CorporateGreyStyle();
        } break;
        case (GUIManager::Styles::CorporateWhite): {
            CorporateWhiteStyle();
        } break;
        default:
            break;
        }
        // Set tesselation error: Smaller value => better tesselation of circles and round corners.
        style.CircleTessellationMaxError = 0.3f;
        // Scale all ImGui style options with current scaling factor
        style.ScaleAllSizes(megamol::gui::gui_scaling.Get());

        this->state.style_changed = false;
    }

    // Update windows
    this->win_collection.Update();

    // Delete window
    if (this->state.win_delete_hash_id != 0) {
        this->win_collection.DeleteWindow(this->state.win_delete_hash_id);
        this->state.win_delete_hash_id = 0;
    }

    // Start new ImGui frame --------------------------------------------------
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

/// DOCKING
#ifdef IMGUI_HAS_DOCK
    // Global Docking Space --------------------------------------------------
    ImGuiStyle& style = ImGui::GetStyle();
    auto child_bg = style.Colors[ImGuiCol_ChildBg];
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    auto global_docking_id =
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    style.Colors[ImGuiCol_ChildBg] = child_bg;

    // Load global docking preset(before first window is drawn!)
    if (this->state.load_docking_preset) {
        this->load_preset_window_docking(global_docking_id);
        this->state.load_docking_preset = false;
    }
#endif

    return true;
}


bool GUIManager::PostDraw() {

    // Early exit when post step should be omitted
    if (!this->state.gui_visible_post) {
        return true;
    }
    // Check for initialized imgui api
    if (this->initialized_api == GUIImGuiAPI::NONE) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] No ImGui API initialized. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    // Check for existing imgui context
    if (this->context == nullptr) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] No valid ImGui context available. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    // Set ImGui context
    ImGui::SetCurrentContext(this->context);
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    ////////// DRAW GUI ///////////////////////////////////////////////////////

    // Main Menu ---------------------------------------------------------------
    this->draw_menu();

    // Draw Windows ------------------------------------------------------------
    const auto func = [&, this](WindowConfiguration& wc) {
        // Update transfer function
        if ((wc.CallbackID() == WindowConfiguration::WINDOW_ID_TRANSFER_FUNCTION) &&
            wc.config.specific.tmp_tfe_reset) {

            this->tfeditor_ptr->SetMinimized(wc.config.specific.tfe_view_minimized);
            this->tfeditor_ptr->SetVertical(wc.config.specific.tfe_view_vertical);

            if (!wc.config.specific.tfe_active_param.empty()) {
                if (auto graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph()) {
                    for (auto& module_ptr : graph_ptr->Modules()) {
                        std::string module_full_name = module_ptr->FullName();
                        for (auto& param : module_ptr->Parameters()) {
                            std::string param_full_name = param.FullNameProject();
                            if (gui_utils::CaseInsensitiveStringCompare(
                                    wc.config.specific.tfe_active_param, param_full_name) &&
                                (param.Type() == ParamType_t::TRANSFERFUNCTION)) {
                                this->tfeditor_ptr->SetConnectedParameter(&param, param_full_name);
                                param.TransferFunctionEditor_ConnectExternal(this->tf_editor_ptr, true);
                            }
                        }
                    }
                }
            }
            wc.config.specific.tmp_tfe_reset = false;
        }

        // Draw window content
        if (wc.Config().show) {

            // Change window flags depending on current view of transfer function editor
            if (wc.CallbackID() == WindowConfiguration::WINDOW_ID_TRANSFER_FUNCTION) {
                if (this->tfeditor_ptr->IsMinimized()) {
                    wc.Config().flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

                } else {
                    wc.Config().flags = ImGuiWindowFlags_AlwaysAutoResize;
                }
                wc.config.specific.tfe_view_minimized = this->tfeditor_ptr->IsMinimized();
                wc.config.specific.tfe_view_vertical = this->tfeditor_ptr->IsVertical();
            }

            ImGui::SetNextWindowBgAlpha(1.0f);
            ImGui::SetNextWindowCollapsed(wc.Config().collapsed, ImGuiCond_Always);

            // Begin Window
            if (!ImGui::Begin(wc.FullWindowTitle().c_str(), &wc.Config().show, wc.Config().flags)) {
                wc.Config().collapsed = ImGui::IsWindowCollapsed();
                ImGui::End(); // early ending
                return;
            }

            // Omit updating size and position of window from imgui for current frame when reset
            bool update_window_by_imgui = !wc.Config().reset_pos_size;
            bool collapsing_changed = false;
            wc.WindowContextMenu(this->state.menu_visible, collapsing_changed);

            // Calling callback drawing window content
            auto cb = this->win_collection.PredefinedWindowCallback(wc.CallbackID());
            if (cb) {
                cb(wc);
            } else if (wc.VolatileCallback()) {
                wc.VolatileCallback()(wc.config.basic);
            } else {
                // Delete window without valid callback
                this->state.win_delete_hash_id = wc.Hash();
                //megamol::core::utility::log::Log::DefaultLog.WriteError(
                //    "[GUI] Missing valid draw callback for GUI window '%s'. [%s, %s, line %d]\n", wc.Name().c_str(),
                //    __FILE__, __FUNCTION__, __LINE__);
            }

            // Saving some of the current window state.
            if (update_window_by_imgui) {
                wc.Config().position = ImGui::GetWindowPos();
                wc.Config().size = ImGui::GetWindowSize();
                if (!collapsing_changed) {
                    wc.Config().collapsed = ImGui::IsWindowCollapsed();
                }
            }

            ImGui::End();
        }
    };
    this->win_collection.EnumWindows(func);

    // Draw pop-ups ------------------------------------------------------------
    this->draw_popups();

    // Draw global parameter widgets -------------------------------------------

    /// TODO - SEPARATE RENDERING OF OPENGL-STUFF DEPENDING ON AVAILABLE API?!

    // Enable OpenGL picking
    /// ! Is only enabled in second frame if interaction objects are added during first frame !
    this->picking_buffer.EnableInteraction(glm::vec2(io.DisplaySize.x, io.DisplaySize.y));

    if (auto graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph()) {
        for (auto& module_ptr : graph_ptr->Modules()) {

            module_ptr->GUIParameterGroups().Draw(module_ptr->Parameters(), "", vislib::math::Ternary::TRI_UNKNOWN,
                false, Parameter::WidgetScope::GLOBAL, this->tf_editor_ptr, nullptr, GUI_INVALID_ID,
                &this->picking_buffer);
        }
    }

    // Disable OpenGL picking
    this->picking_buffer.DisableInteraction();

    ///////////////////////////////////////////////////////////////////////////

    // Render the current ImGui frame ------------------------------------------
    glViewport(0, 0, static_cast<GLsizei>(io.DisplaySize.x), static_cast<GLsizei>(io.DisplaySize.y));
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Reset hotkeys ----------------------------------------------------------
    for (auto& h : this->hotkeys) {
        h.is_pressed = false;
    }

    // Hide GUI if it is currently shown --------------------------------------
    if (this->state.gui_visible) {
        if (this->state.gui_hide_next_frame == 2) {
            // First frame
            this->state.gui_hide_next_frame--;
            // Save 'open' state of windows for later restore. Closing all windows before omitting GUI rendering is
            // required to set right ImGui state for mouse handling
            this->state.gui_restore_hidden_windows.clear();
            const auto func = [&, this](WindowConfiguration& wc) {
                if (wc.Config().show) {
                    this->state.gui_restore_hidden_windows.push_back(wc.Name());
                    wc.Config().show = false;
                }
            };
            this->win_collection.EnumWindows(func);
        } else if (this->state.gui_hide_next_frame == 1) {
            // Second frame
            this->state.gui_hide_next_frame = 0;
            this->state.gui_visible = false;
        }
    }

    // Apply new gui scale -----------------------------------------------------
    if (megamol::gui::gui_scaling.ConsumePendingChange()) {

        // Reload ImGui style options
        this->state.style_changed = true;

        // Scale all windows
        if (this->state.rescale_windows) {
            // Do not adjust window scale after loading from project file (window size is already fine)
            const auto size_func = [&, this](WindowConfiguration& wc) {
                wc.Config().reset_size *= megamol::gui::gui_scaling.TransitionFactor();
                wc.Config().size *= megamol::gui::gui_scaling.TransitionFactor();
                wc.Config().reset_pos_size = true;
            };
            this->win_collection.EnumWindows(size_func);
            this->state.rescale_windows = false;
        }

        // Reload and scale all fonts
        this->state.load_fonts = true;
    }

    // Loading new font -------------------------------------------------------
    // (after first imgui frame for default fonts being available)
    if (this->state.font_apply) {
        bool load_success = false;
        if (megamol::core::utility::FileUtils::FileWithExtensionExists<std::string>(
                this->state.font_file_name, std::string("ttf"))) {
            ImFontConfig config;
            config.OversampleH = 4;
            config.OversampleV = 4;
            config.GlyphRanges = this->state.font_utf8_ranges.data();
            gui_utils::Utf8Encode(this->state.font_file_name);
            if (io.Fonts->AddFontFromFileTTF(this->state.font_file_name.c_str(),
                    static_cast<float>(this->state.font_size), &config) != nullptr) {

                bool font_api_load_success = false;
                switch (this->initialized_api) {
                case (GUIImGuiAPI::OPEN_GL): {
                    font_api_load_success = ImGui_ImplOpenGL3_CreateFontsTexture();
                } break;
                default: {
                    megamol::core::utility::log::Log::DefaultLog.WriteError(
                        "[GUI] ImGui API is not supported. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
                } break;
                }
                // Load last added font
                if (font_api_load_success) {
                    io.FontDefault = io.Fonts->Fonts[(io.Fonts->Fonts.Size - 1)];
                    load_success = true;
                }
            }
            gui_utils::Utf8Decode(this->state.font_file_name);
        } else if (this->state.font_file_name != "<unknown>") {
            std::string imgui_font_string =
                this->state.font_file_name + ", " + std::to_string(this->state.font_size) + "px";
            for (unsigned int n = this->state.graph_fonts_reserved; n < static_cast<unsigned int>(io.Fonts->Fonts.Size);
                 n++) {
                std::string font_name = std::string(io.Fonts->Fonts[n]->GetDebugName());
                gui_utils::Utf8Decode(font_name);
                if (font_name == imgui_font_string) {
                    io.FontDefault = io.Fonts->Fonts[n];
                    load_success = true;
                }
            }
        }
        // if (!load_success) {
        //    megamol::core::utility::log::Log::DefaultLog.WriteWarn(
        //        "[GUI] Unable to load font '%s' with size %d (NB: ImGui default font ProggyClean.ttf can only be "
        //        "loaded with predefined size 13). [%s, %s, line %d]\n",
        //        this->state.font_file_name.c_str(), this->state.font_size, __FILE__, __FUNCTION__, __LINE__);
        //}
        this->state.font_apply = false;
    }

    return true;
}


bool GUIManager::OnKey(core::view::Key key, core::view::KeyAction action, core::view::Modifiers mods) {

    ImGui::SetCurrentContext(this->context);

    ImGuiIO& io = ImGui::GetIO();

    bool last_return_key = io.KeysDown[static_cast<size_t>(core::view::Key::KEY_ENTER)];
    bool last_num_enter_key = io.KeysDown[static_cast<size_t>(core::view::Key::KEY_KP_ENTER)];

    auto keyIndex = static_cast<size_t>(key);
    switch (action) {
    case core::view::KeyAction::PRESS:
        io.KeysDown[keyIndex] = true;
        break;
    case core::view::KeyAction::RELEASE:
        io.KeysDown[keyIndex] = false;
        break;
    default:
        break;
    }
    io.KeyCtrl = mods.test(core::view::Modifier::CTRL);
    io.KeyShift = mods.test(core::view::Modifier::SHIFT);
    io.KeyAlt = mods.test(core::view::Modifier::ALT);

    // Pass NUM 'Enter' as alternative for 'Return' to ImGui
    bool cur_return_key = ImGui::IsKeyDown(static_cast<int>(core::view::Key::KEY_ENTER));
    bool cur_num_enter_key = ImGui::IsKeyDown(static_cast<int>(core::view::Key::KEY_KP_ENTER));
    bool return_pressed = (!last_return_key && cur_return_key);
    bool enter_pressed = (!last_num_enter_key && cur_num_enter_key);
    io.KeysDown[static_cast<size_t>(core::view::Key::KEY_ENTER)] = (return_pressed || enter_pressed);

    bool hotkeyPressed = false;

    // GUI
    for (auto& h : this->hotkeys) {
        if (this->is_hotkey_pressed(h.keycode)) {
            h.is_pressed = true;
            hotkeyPressed = true;
        }
    }
    // Configurator
    for (auto& h : this->configurator_ptr->GetHotkeys()) {
        if (this->is_hotkey_pressed(h.keycode)) {
            h.is_pressed = true;
            hotkeyPressed = true;
        }
    }
    if (hotkeyPressed)
        return true;

    // Check for additional text modification hotkeys
    if (action == core::view::KeyAction::RELEASE) {
        io.KeysDown[static_cast<size_t>(GuiTextModHotkeys::CTRL_A)] = false;
        io.KeysDown[static_cast<size_t>(GuiTextModHotkeys::CTRL_C)] = false;
        io.KeysDown[static_cast<size_t>(GuiTextModHotkeys::CTRL_V)] = false;
        io.KeysDown[static_cast<size_t>(GuiTextModHotkeys::CTRL_X)] = false;
        io.KeysDown[static_cast<size_t>(GuiTextModHotkeys::CTRL_Y)] = false;
        io.KeysDown[static_cast<size_t>(GuiTextModHotkeys::CTRL_Z)] = false;
    }
    hotkeyPressed = true;
    if (io.KeyCtrl && ImGui::IsKeyDown(static_cast<int>(core::view::Key::KEY_A))) {
        keyIndex = static_cast<size_t>(GuiTextModHotkeys::CTRL_A);
    } else if (io.KeyCtrl && ImGui::IsKeyDown(static_cast<int>(core::view::Key::KEY_C))) {
        keyIndex = static_cast<size_t>(GuiTextModHotkeys::CTRL_C);
    } else if (io.KeyCtrl && ImGui::IsKeyDown(static_cast<int>(core::view::Key::KEY_V))) {
        keyIndex = static_cast<size_t>(GuiTextModHotkeys::CTRL_V);
    } else if (io.KeyCtrl && ImGui::IsKeyDown(static_cast<int>(core::view::Key::KEY_X))) {
        keyIndex = static_cast<size_t>(GuiTextModHotkeys::CTRL_X);
    } else if (io.KeyCtrl && ImGui::IsKeyDown(static_cast<int>(core::view::Key::KEY_Y))) {
        keyIndex = static_cast<size_t>(GuiTextModHotkeys::CTRL_Y);
    } else if (io.KeyCtrl && ImGui::IsKeyDown(static_cast<int>(core::view::Key::KEY_Z))) {
        keyIndex = static_cast<size_t>(GuiTextModHotkeys::CTRL_Z);
    } else {
        hotkeyPressed = false;
    }
    if (hotkeyPressed && (action == core::view::KeyAction::PRESS)) {
        io.KeysDown[keyIndex] = true;
        return true;
    }

    // Hotkeys for showing/hiding window(s)
    const auto windows_func = [&](WindowConfiguration& wc) {
        bool windowHotkeyPressed = this->is_hotkey_pressed(wc.Config().hotkey);
        if (windowHotkeyPressed) {
            wc.Config().show = !wc.Config().show;
        }
        hotkeyPressed |= windowHotkeyPressed;
    };
    this->win_collection.EnumWindows(windows_func);
    if (hotkeyPressed)
        return true;

    // Always consume keyboard input if requested by any imgui widget (e.g. text input).
    // User expects hotkey priority of text input thus needs to be processed before parameter hotkeys.
    if (io.WantTextInput) { /// io.WantCaptureKeyboard
        return true;
    }

    // Check for parameter hotkeys
    hotkeyPressed = false;
    if (auto graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph()) {
        for (auto& module_ptr : graph_ptr->Modules()) {
            // Break loop after first occurrence of parameter hotkey
            if (hotkeyPressed) {
                break;
            }
            for (auto& param : module_ptr->Parameters()) {
                if (param.Type() == ParamType_t::BUTTON) {
                    auto keyCode = param.GetStorage<megamol::core::view::KeyCode>();
                    if (this->is_hotkey_pressed(keyCode)) {
                        // Sync directly button action to parameter in core
                        /// Does not require syncing of graphs
                        if (param.CoreParamPtr() != nullptr) {
                            param.CoreParamPtr()->setDirty();
                        }
                        /// param.ForceSetValueDirty();
                        hotkeyPressed = true;
                    }
                }
            }
        }
    }

    return hotkeyPressed;
}


bool GUIManager::OnChar(unsigned int codePoint) {
    ImGui::SetCurrentContext(this->context);

    ImGuiIO& io = ImGui::GetIO();
    io.ClearInputCharacters();
    if (codePoint > 0 && codePoint < 0x10000) {
        io.AddInputCharacter((unsigned short) codePoint);
    }

    return false;
}


bool GUIManager::OnMouseMove(double x, double y) {
    ImGui::SetCurrentContext(this->context);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(static_cast<float>(x), static_cast<float>(y));

    auto hoverFlags = ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenDisabled |
                      ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;

    // Always consumed if any imgui windows is hovered.
    bool consumed = ImGui::IsWindowHovered(hoverFlags);
    if (!consumed) {
        consumed = this->picking_buffer.ProcessMouseMove(x, y);
    }

    return consumed;
}


bool GUIManager::OnMouseButton(
    core::view::MouseButton button, core::view::MouseButtonAction action, core::view::Modifiers mods) {

    ImGui::SetCurrentContext(this->context);

    bool down = (action == core::view::MouseButtonAction::PRESS);
    auto buttonIndex = static_cast<size_t>(button);
    ImGuiIO& io = ImGui::GetIO();

    auto hoverFlags = ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenDisabled |
                      ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;

    io.MouseDown[buttonIndex] = down;

    // Always consumed if any imgui windows is hovered.
    bool consumed = ImGui::IsWindowHovered(hoverFlags);
    if (!consumed) {
        consumed = this->picking_buffer.ProcessMouseClick(button, action, mods);
    }

    return consumed;
}


bool GUIManager::OnMouseScroll(double dx, double dy) {
    ImGui::SetCurrentContext(this->context);

    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheelH += (float) dx;
    io.MouseWheel += (float) dy;

    auto hoverFlags = ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenDisabled |
                      ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;

    // Always consumed if any imgui windows is hovered.
    bool consumed = ImGui::IsWindowHovered(hoverFlags);
    return consumed;
}


bool megamol::gui::GUIManager::GetTriggeredScreenshot() {

    bool trigger_screenshot = this->state.screenshot_triggered;
    this->state.screenshot_triggered = false;
    if (trigger_screenshot) {
        this->create_not_existing_png_filepath(this->state.screenshot_filepath);
        if (this->state.screenshot_filepath.empty()) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "[GUI] Filename for screenshot should not be empty. [%s, %s, line %d]\n", __FILE__, __FUNCTION__,
                __LINE__);
            trigger_screenshot = false;
        }
    }

    return trigger_screenshot;
}


std::string megamol::gui::GUIManager::GetProjectLoadRequest() {

    auto project_file_name = this->state.request_load_projet_file;
    this->state.request_load_projet_file.clear();
    return project_file_name;
}


void megamol::gui::GUIManager::SetScale(float scale) {
    megamol::gui::gui_scaling.Set(scale);
    if (megamol::gui::gui_scaling.PendingChange()) {
        // Additionally trigger reload of currently used font
        this->state.font_apply = true;
        this->state.font_size = static_cast<int>(
            static_cast<float>(this->state.font_size) * (megamol::gui::gui_scaling.TransitionFactor()));
        // Additionally resize all windows
        this->state.rescale_windows = true;
    }
}


void megamol::gui::GUIManager::SetClipboardFunc(const char* (*get_clipboard_func)(void* user_data),
    void (*set_clipboard_func)(void* user_data, const char* string), void* user_data) {

    if (this->context != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        io.SetClipboardTextFn = set_clipboard_func;
        io.GetClipboardTextFn = get_clipboard_func;
        io.ClipboardUserData = user_data;
    }
}


bool megamol::gui::GUIManager::SynchronizeRunningGraph(megamol::core::MegaMolGraph& megamol_graph, megamol::core::CoreInstance& core_instance) {

    // Synchronization is not required when no gui element is visible
    if (!this->state.gui_visible)
        return true;

    // 1) Load all known calls from core instance ONCE ---------------------------
    if (!this->configurator_ptr->GetGraphCollection().LoadCallStock(core_instance)) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] Failed to load call stock once. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    // Load all known modules from core instance ONCE
    if (!this->configurator_ptr->GetGraphCollection().LoadModuleStock(core_instance)) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] Failed to load module stock once. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    bool synced = false;
    bool sync_success = false;
    GraphPtr_t graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph();
    // 2a) Either synchronize GUI Graph -> Core Graph ... ---------------------
    if (!synced && (graph_ptr != nullptr)) {
        bool graph_sync_success = true;

        Graph::QueueAction action;
        Graph::QueueData data;
        while (graph_ptr->PopSyncQueue(action, data)) {
            synced = true;
            switch (action) {
            case (Graph::QueueAction::ADD_MODULE): {
                graph_sync_success &= megamol_graph.CreateModule(data.class_name, data.name_id);
            } break;
            case (Graph::QueueAction::RENAME_MODULE): {
                bool rename_success = megamol_graph.RenameModule(data.name_id, data.rename_id);
                graph_sync_success &= rename_success;
            } break;
            case (Graph::QueueAction::DELETE_MODULE): {
                graph_sync_success &= megamol_graph.DeleteModule(data.name_id);
            } break;
            case (Graph::QueueAction::ADD_CALL): {
                graph_sync_success &= megamol_graph.CreateCall(data.class_name, data.caller, data.callee);
            } break;
            case (Graph::QueueAction::DELETE_CALL): {
                graph_sync_success &= megamol_graph.DeleteCall(data.caller, data.callee);
            } break;
            case (Graph::QueueAction::CREATE_GRAPH_ENTRY): {
                megamol_graph.SetGraphEntryPoint(data.name_id);
            } break;
            case (Graph::QueueAction::REMOVE_GRAPH_ENTRY): {
                megamol_graph.RemoveGraphEntryPoint(data.name_id);
            } break;
            default:
                break;
            }
        }
        if (!graph_sync_success) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "[GUI] Failed to synchronize gui graph with core graph. [%s, %s, line %d]\n", __FILE__, __FUNCTION__,
                __LINE__);
        }
        sync_success &= graph_sync_success;
    }

    // 2b) ... OR (exclusive or) synchronize Core Graph -> GUI Graph ----------
    if (!synced) {
        // Creates new graph at first call
        ImGuiID running_graph_uid = (graph_ptr != nullptr) ? (graph_ptr->UID()) : (GUI_INVALID_ID);
        bool graph_sync_success = this->configurator_ptr->GetGraphCollection().LoadUpdateProjectFromCore(
            running_graph_uid, megamol_graph);
        if (!graph_sync_success) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "[GUI] Failed to synchronize core graph with gui graph. [%s, %s, line %d]\n", __FILE__, __FUNCTION__,
                __LINE__);
        }

        // Check for new GUI state
        if (!this->state.new_gui_state.empty()) {
            this->state_from_string(this->state.new_gui_state);
            this->state.new_gui_state.clear();
        }

        // Check for new script path name
        if (graph_sync_success) {
            if (auto graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph()) {
                std::string script_filename;
                // Get project filename from lua state of frontend service
                if (!this->state.project_script_paths.empty()) {
                    script_filename = this->state.project_script_paths.front();
                }
                // Load GUI state from project file when project file changed
                if (!script_filename.empty()) {
                    graph_ptr->SetFilename(script_filename, false);
                }
            }
        }

        sync_success &= graph_sync_success;
    }

    // 3) Synchronize parameter values -------------------------------------------
    if (graph_ptr != nullptr) {
        bool param_sync_success = true;
        for (auto& module_ptr : graph_ptr->Modules()) {
            for (auto& param : module_ptr->Parameters()) {

                // Try to connect gui parameters to newly created parameters of core modules
                if (param.CoreParamPtr().IsNull()) {
                    auto module_name = module_ptr->FullName();
                    megamol::core::Module* core_module_ptr = nullptr;
                    core_module_ptr = megamol_graph.FindModule(module_name).get();
                    // Connect pointer of new parameters of core module to parameters in gui module
                    if (core_module_ptr != nullptr) {
                        auto se = core_module_ptr->ChildList_End();
                        for (auto si = core_module_ptr->ChildList_Begin(); si != se; ++si) {
                            auto param_slot = dynamic_cast<megamol::core::param::ParamSlot*>((*si).get());
                            if (param_slot != nullptr) {
                                std::string param_full_name(param_slot->FullName().PeekBuffer());
                                for (auto& parameter : module_ptr->Parameters()) {
                                    if (gui_utils::CaseInsensitiveStringCompare(
                                            parameter.FullNameCore(), param_full_name)) {
                                        megamol::gui::Parameter::ReadNewCoreParameterToExistingParameter(
                                            (*param_slot), parameter, true, false, true);
                                    }
                                }
                            }
                        }
                    }
#ifdef GUI_VERBOSE
                    if (param.CoreParamPtr().IsNull()) {
                        megamol::core::utility::log::Log::DefaultLog.WriteError(
                            "[GUI] Unable to connect core parameter to gui parameter. [%s, %s, line %d]\n", __FILE__,
                            __FUNCTION__, __LINE__);
                    }
#endif // GUI_VERBOSE
                }

                if (!param.CoreParamPtr().IsNull()) {
                    // Write changed gui state to core parameter
                    if (param.IsGUIStateDirty()) {
                        param_sync_success &=
                            megamol::gui::Parameter::WriteCoreParameterGUIState(param, param.CoreParamPtr());
                        param.ResetGUIStateDirty();
                    }
                    // Write changed parameter value to core parameter
                    if (param.IsValueDirty()) {
                        param_sync_success &=
                            megamol::gui::Parameter::WriteCoreParameterValue(param, param.CoreParamPtr());
                        param.ResetValueDirty();
                    }
                    // Read current parameter value and GUI state fro core parameter
                    param_sync_success &= megamol::gui::Parameter::ReadCoreParameterToParameter(
                        param.CoreParamPtr(), param, false, false);
                }
            }
        }
#ifdef GUI_VERBOSE
        if (!param_sync_success) {
            megamol::core::utility::log::Log::DefaultLog.WriteWarn(
                "[GUI] Failed to synchronize parameter values. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        }
#endif // GUI_VERBOSE
        sync_success &= param_sync_success;
    }
    return sync_success;
}


bool GUIManager::create_context() {

    if (this->initialized_api != GUIImGuiAPI::NONE) {
        megamol::core::utility::log::Log::DefaultLog.WriteWarn(
            "[GUI] ImGui context has alreday been created. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return true;
    }

    // Check for existing context and share FontAtlas with new context (required by ImGui).
    bool other_context_exists = (ImGui::GetCurrentContext() != nullptr);
    ImFontAtlas* font_atlas = nullptr;
    ImFont* default_font = nullptr;
    // Handle multiple ImGui contexts.
    if (other_context_exists) {
        ImGuiIO& current_io = ImGui::GetIO();
        font_atlas = current_io.Fonts;
        default_font = current_io.FontDefault;
        ImGui::GetCurrentContext()->FontAtlasOwnedByContext = false;
    }

    // Create ImGui context ---------------------------------------------------
    IMGUI_CHECKVERSION();
    this->context = ImGui::CreateContext(font_atlas);
    if (this->context == nullptr) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] Unable to create ImGui context. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    // Style settings ---------------------------------------------------------
    ImGui::SetColorEditOptions(ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB |
                               ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaBar |
                               ImGuiColorEditFlags_AlphaPreview);

    // IO settings ------------------------------------------------------------
    ImGuiIO& io = ImGui::GetIO();
    io.IniSavingRate = 5.0f;                              //  in seconds - unused
    io.IniFilename = nullptr;                             // "imgui.ini" - disabled, using own window settings profile
    io.LogFilename = nullptr;                             // "imgui_log.txt" - disabled
    io.FontAllowUserScaling = false;                      // disable font scaling using ctrl + mouse wheel
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // allow keyboard navigation
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // GetMouseCursor() is processed in frontend service

/// DOCKING https://github.com/ocornut/imgui/issues/2109
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // enable window docking
    io.ConfigDockingWithShift = true;                 // activate docking on pressing 'shift'
#endif

/// MULTI-VIEWPORT https://github.com/ocornut/imgui/issues/1542
#ifdef IMGUI_HAS_VIEWPORT
    /*
    #include "GLFW/glfw3.h"
    #include "imgui_impl_glfw.h"*
    * See ...\build\_deps\imgui-src\examples\example_glfw_opengl3\main.cpp for required setup
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // enable multi-viewport
    // Add ...\plugins\gui\CMakeLists.txt
        #GLFW
        if (USE_GLFW)
            require_external(glfw3) target_link_libraries(${PROJECT_NAME} PRIVATE glfw3) endif()
    // (get glfw_win via WindowManipulation glfw resource)
    ImGui_ImplGlfw_InitForOpenGL(glfw_win, true);
    // ...
    ImGui_ImplGlfw_NewFrame();
    */
#endif

    // ImGui Key Map
    io.KeyMap[ImGuiKey_Tab] = static_cast<int>(core::view::Key::KEY_TAB);
    io.KeyMap[ImGuiKey_LeftArrow] = static_cast<int>(core::view::Key::KEY_LEFT);
    io.KeyMap[ImGuiKey_RightArrow] = static_cast<int>(core::view::Key::KEY_RIGHT);
    io.KeyMap[ImGuiKey_UpArrow] = static_cast<int>(core::view::Key::KEY_UP);
    io.KeyMap[ImGuiKey_DownArrow] = static_cast<int>(core::view::Key::KEY_DOWN);
    io.KeyMap[ImGuiKey_PageUp] = static_cast<int>(core::view::Key::KEY_PAGE_UP);
    io.KeyMap[ImGuiKey_PageDown] = static_cast<int>(core::view::Key::KEY_PAGE_DOWN);
    io.KeyMap[ImGuiKey_Home] = static_cast<int>(core::view::Key::KEY_HOME);
    io.KeyMap[ImGuiKey_End] = static_cast<int>(core::view::Key::KEY_END);
    io.KeyMap[ImGuiKey_Insert] = static_cast<int>(core::view::Key::KEY_INSERT);
    io.KeyMap[ImGuiKey_Delete] = static_cast<int>(core::view::Key::KEY_DELETE);
    io.KeyMap[ImGuiKey_Backspace] = static_cast<int>(core::view::Key::KEY_BACKSPACE);
    io.KeyMap[ImGuiKey_Space] = static_cast<int>(core::view::Key::KEY_SPACE);
    io.KeyMap[ImGuiKey_Enter] = static_cast<int>(core::view::Key::KEY_ENTER);
    io.KeyMap[ImGuiKey_Escape] = static_cast<int>(core::view::Key::KEY_ESCAPE);
    io.KeyMap[ImGuiKey_A] = static_cast<int>(GuiTextModHotkeys::CTRL_A);
    io.KeyMap[ImGuiKey_C] = static_cast<int>(GuiTextModHotkeys::CTRL_C);
    io.KeyMap[ImGuiKey_V] = static_cast<int>(GuiTextModHotkeys::CTRL_V);
    io.KeyMap[ImGuiKey_X] = static_cast<int>(GuiTextModHotkeys::CTRL_X);
    io.KeyMap[ImGuiKey_Y] = static_cast<int>(GuiTextModHotkeys::CTRL_Y);
    io.KeyMap[ImGuiKey_Z] = static_cast<int>(GuiTextModHotkeys::CTRL_Z);

    // Init global state -------------------------------------------------------
    this->init_state();

    this->configurator_ptr = this->win_collection.GetWindow(WindowConfiguration::WINDOW_ID_CONFIGURATOR);
    if (this->configurator_ptr == nullptr) {
        megamol::core::utility::log::Log::DefaultLog.WriteWarn(
                "[GUI] Unable to find configurator window. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    this->tfeditor_ptr = this->win_collection.GetWindow(WindowConfiguration::WINDOW_ID_TRANSFER_FUNCTION);
    if (this->tfeditor_ptr == nullptr) {
        megamol::core::utility::log::Log::DefaultLog.WriteWarn(
                "[GUI] Unable to find transfer function editor window. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    // Adding additional utf-8 glyph ranges
    // (there is no error if glyph has no representation in font atlas)
    this->state.font_utf8_ranges.clear();
    this->state.font_utf8_ranges.emplace_back(0x0020);
    this->state.font_utf8_ranges.emplace_back(0x03FF); // Basic Latin + Latin Supplement + Greek Alphabet
    this->state.font_utf8_ranges.emplace_back(0x20AC);
    this->state.font_utf8_ranges.emplace_back(0x20AC); // Euro
    this->state.font_utf8_ranges.emplace_back(0x2122);
    this->state.font_utf8_ranges.emplace_back(0x2122); // TM
    this->state.font_utf8_ranges.emplace_back(0x212B);
    this->state.font_utf8_ranges.emplace_back(0x212B); // Angstroem
    this->state.font_utf8_ranges.emplace_back(0x0391);
    this->state.font_utf8_ranges.emplace_back(0); // (range termination)

    // Load initial fonts only once for all imgui contexts --------------------
    if (other_context_exists) {

        // Fonts are already loaded
        if (default_font != nullptr) {
            io.FontDefault = default_font;
        } else {
            // ... else default font is font loaded after configurator fonts -> Index equals number of graph fonts.
            auto default_font_index = static_cast<int>(this->configurator_ptr->GetGraphFontScalings().size());
            default_font_index = std::min(default_font_index, io.Fonts->Fonts.Size - 1);
            io.FontDefault = io.Fonts->Fonts[default_font_index];
        }

    } else {
        this->state.load_fonts = true;
    }

    return true;
}


bool GUIManager::destroy_context() {

    if (this->initialized_api != GUIImGuiAPI::NONE) {
        if (this->context != nullptr) {

            // Handle multiple ImGui contexts.
            if (megamol::gui::gui_context_count < 2) {
                ImGui::SetCurrentContext(this->context);
                // Shutdown API only if only one context is left
                switch (this->initialized_api) {
                case (GUIImGuiAPI::OPEN_GL):
                    ImGui_ImplOpenGL3_Shutdown();
                    break;
                default:
                    break;
                }
                // Last context should delete font atlas
                ImGui::GetCurrentContext()->FontAtlasOwnedByContext = true;
            }

            ImGui::DestroyContext(this->context);
            megamol::gui::gui_context_count--;
            megamol::core::utility::log::Log::DefaultLog.WriteInfo("[GUI] Destroyed ImGui context.");
        }
        this->context = nullptr;
        this->initialized_api = GUIImGuiAPI::NONE;
    }

    return true;
}


void megamol::gui::GUIManager::load_default_fonts() {

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const auto graph_font_scalings = this->configurator_ptr->GetGraphFontScalings();
    this->state.graph_fonts_reserved = graph_font_scalings.size();

    const float default_font_size = (12.0f * megamol::gui::gui_scaling.Get());
    ImFontConfig config;
    config.OversampleH = 4;
    config.OversampleV = 4;
    config.GlyphRanges = this->state.font_utf8_ranges.data();

    // Get other known fonts
    std::vector<std::string> font_paths;
    std::string configurator_font_path;
    std::string default_font_path;

    auto get_preset_font_path = [&](const std::string& directory) {
        std::string font_path =
            megamol::core::utility::FileUtils::SearchFileRecursive(directory, GUI_DEFAULT_FONT_ROBOTOSANS);
        if (!font_path.empty()) {
            font_paths.emplace_back(font_path);
            configurator_font_path = font_path;
            default_font_path = font_path;
        }
        font_path = megamol::core::utility::FileUtils::SearchFileRecursive(directory, GUI_DEFAULT_FONT_SOURCECODEPRO);
        if (!font_path.empty()) {
            font_paths.emplace_back(font_path);
        }
    };

    for (auto& resource_directory : megamol::gui::gui_resource_paths) {
        get_preset_font_path(resource_directory);
    }

    // Configurator Graph Font: Add default font at first n indices for exclusive use in configurator graph.
    /// Workaround: Using different font sizes for different graph zooming factors to improve font readability when
    /// zooming.
    if (configurator_font_path.empty()) {
        for (unsigned int i = 0; i < this->state.graph_fonts_reserved; i++) {
            io.Fonts->AddFontDefault(&config);
        }
    } else {
        for (unsigned int i = 0; i < this->state.graph_fonts_reserved; i++) {
            io.Fonts->AddFontFromFileTTF(
                configurator_font_path.c_str(), default_font_size * graph_font_scalings[i], &config);
        }
    }

    // Add other fonts for gui.
    io.Fonts->AddFontDefault(&config);
    io.FontDefault = io.Fonts->Fonts[(io.Fonts->Fonts.Size - 1)];
    for (auto& font_path : font_paths) {
        io.Fonts->AddFontFromFileTTF(font_path.c_str(), default_font_size, &config);
        if (default_font_path == font_path) {
            io.FontDefault = io.Fonts->Fonts[(io.Fonts->Fonts.Size - 1)];
        }
    }

    switch (this->initialized_api) {
    case (GUIImGuiAPI::NONE): {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] Fonts can only be loaded after API was initialized. [%s, %s, line %d]\n", __FILE__, __FUNCTION__,
            __LINE__);
    } break;
    case (GUIImGuiAPI::OPEN_GL): {
        ImGui_ImplOpenGL3_CreateFontsTexture();
    } break;
    default: {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] ImGui API is not supported. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
    } break;
    }
}


void GUIManager::draw_menu() {

    if (!this->state.menu_visible)
        return;

    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::BeginMainMenuBar();

    // FILE -------------------------------------------------------------------
    if (ImGui::BeginMenu("File")) {

        if (ImGui::MenuItem("Load Project",
                this->hotkeys[GUIManager::GuiHotkeyIndex::LOAD_PROJECT].keycode.ToString().c_str())) {
            this->state.open_popup_load = true;
        }
        if (ImGui::MenuItem(
                "Save Project", this->hotkeys[GUIManager::GuiHotkeyIndex::SAVE_PROJECT].keycode.ToString().c_str())) {
            this->state.open_popup_save = true;
        }
        if (ImGui::MenuItem(
                "Exit", this->hotkeys[GUIManager::GuiHotkeyIndex::EXIT_PROGRAM].keycode.ToString().c_str())) {
            this->state.shutdown_triggered = true;
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();

    // WINDOWS ----------------------------------------------------------------
    if (ImGui::BeginMenu("Windows")) {
        ImGui::MenuItem("Menu", this->hotkeys[GUIManager::GuiHotkeyIndex::MENU].keycode.ToString().c_str(),
            &this->state.menu_visible);
        const auto func = [&, this](WindowConfiguration& wc) {
            bool registered_window = (wc.Config().hotkey.key != core::view::Key::KEY_UNKNOWN);
            if (registered_window) {
                ImGui::MenuItem(wc.Name().c_str(), wc.Config().hotkey.ToString().c_str(), &wc.Config().show);
            } else {
                // Custom unregistered parameter window
                if (ImGui::BeginMenu(wc.Name().c_str())) {
                    std::string menu_label = "Show";
                    if (wc.Config().show)
                        menu_label = "Hide";
                    if (ImGui::MenuItem(menu_label.c_str(), wc.Config().hotkey.ToString().c_str(), nullptr)) {
                        wc.Config().show = !wc.Config().show;
                    }
                    // Enable option to delete custom newly created parameter windows
                    if (wc.WindowID() == WindowConfiguration::WINDOW_ID_PARAMETERS) {
                        if (ImGui::MenuItem("Delete Window")) {
                            this->state.win_delete_hash_id = wc.Hash();
                        }
                    }
                    ImGui::EndMenu();
                }
            }
        };
        this->win_collection.EnumWindows(func);

        ImGui::Separator();

        if (ImGui::MenuItem("Show/Hide All Windows",
                this->hotkeys[GUIManager::GuiHotkeyIndex::SHOW_HIDE_GUI].keycode.ToString().c_str())) {
            this->state.gui_hide_next_frame = 2;
        }

/// DOCKING
#ifdef IMGUI_HAS_DOCK
        if (ImGui::MenuItem("Windows Docking Preset")) {
            this->state.load_docking_preset = true;
        }
#endif
        ImGui::EndMenu();
    }
    ImGui::Separator();

    // SCREENSHOT -------------------------------------------------------------
    if (ImGui::BeginMenu("Screenshot")) {
        this->create_not_existing_png_filepath(this->state.screenshot_filepath);
        if (ImGui::MenuItem("Select Filename", this->state.screenshot_filepath.c_str())) {
            this->state.open_popup_screenshot = true;
        }
        if (ImGui::MenuItem("Trigger",
                this->hotkeys[GUIManager::GuiHotkeyIndex::TRIGGER_SCREENSHOT].keycode.ToString().c_str())) {
            this->state.screenshot_triggered = true;
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();

    // RENDER -----------------------------------------------------------------
    if (ImGui::BeginMenu("Projects")) {
        for (auto& graph_ptr : this->configurator_ptr->GetGraphCollection().GetGraphs()) {
            bool running = graph_ptr->IsRunning();
            std::string button_label = "graph_running_button" + std::to_string(graph_ptr->UID());
            if (megamol::gui::ButtonWidgets::OptionButton(button_label, "", running)) {
                if (!running) {
                    this->configurator_ptr->GetGraphCollection().RequestNewRunningGraph(graph_ptr->UID());
                }
            }
            std::string tooltip_str = "Click to run project";
            if (running) {
                tooltip_str = "Project is running";
            }
            this->tooltip.ToolTip(tooltip_str);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();

            if (ImGui::BeginMenu(graph_ptr->Name().c_str(), running)) {
                ImGui::TextDisabled("Graph Entry");
                ImGui::Separator();
                for (auto& module_ptr : graph_ptr->Modules()) {
                    if (module_ptr->IsView()) {
                        if (ImGui::MenuItem(module_ptr->FullName().c_str(), "", module_ptr->IsGraphEntry())) {
                            if (!module_ptr->IsGraphEntry()) {
                                // Remove all graph entries
                                for (auto& rem_module_ptr : graph_ptr->Modules()) {
                                    if (rem_module_ptr->IsView() && rem_module_ptr->IsGraphEntry()) {
                                        rem_module_ptr->SetGraphEntryName("");
                                        Graph::QueueData queue_data;
                                        queue_data.name_id = rem_module_ptr->FullName();
                                        graph_ptr->PushSyncQueue(
                                            Graph::QueueAction::REMOVE_GRAPH_ENTRY, queue_data);
                                    }
                                }
                                // Add new graph entry
                                module_ptr->SetGraphEntryName(graph_ptr->GenerateUniqueGraphEntryName());
                                Graph::QueueData queue_data;
                                queue_data.name_id = module_ptr->FullName();
                                graph_ptr->PushSyncQueue(Graph::QueueAction::CREATE_GRAPH_ENTRY, queue_data);
                            } else {
                                module_ptr->SetGraphEntryName("");
                                Graph::QueueData queue_data;
                                queue_data.name_id = module_ptr->FullName();
                                graph_ptr->PushSyncQueue(Graph::QueueAction::REMOVE_GRAPH_ENTRY, queue_data);
                            }
                        }
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Toggle Graph Entry",
                        this->hotkeys[GUIManager::GuiHotkeyIndex::TOGGLE_GRAPH_ENTRY].keycode.ToString().c_str())) {
                    this->state.toggle_graph_entry = true;
                }

                ImGui::EndMenu();
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();

    // SETTINGS ---------------------------------------------------------------
    if (ImGui::BeginMenu("Settings")) {

        if (ImGui::BeginMenu("Style")) {

            if (ImGui::MenuItem("ImGui Dark Colors", nullptr, (this->state.style == GUIManager::Styles::DarkColors))) {
                this->state.style = GUIManager::Styles::DarkColors;
                this->state.style_changed = true;
            }
            if (ImGui::MenuItem("ImGui LightColors", nullptr, (this->state.style == GUIManager::Styles::LightColors))) {
                this->state.style = GUIManager::Styles::LightColors;
                this->state.style_changed = true;
            }
            if (ImGui::MenuItem("Corporate Gray", nullptr, (this->state.style == GUIManager::Styles::CorporateGray))) {
                this->state.style = GUIManager::Styles::CorporateGray;
                this->state.style_changed = true;
            }
            if (ImGui::MenuItem(
                    "Corporate White", nullptr, (this->state.style == GUIManager::Styles::CorporateWhite))) {
                this->state.style = GUIManager::Styles::CorporateWhite;
                this->state.style_changed = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Font")) {

            ImGuiIO& io = ImGui::GetIO();
            ImFont* font_current = ImGui::GetFont();
            if (ImGui::BeginCombo("Select Available Font", font_current->GetDebugName())) {
                /// first fonts until index this->graph_fonts_reserved are exclusively used by graph in configurator
                for (int n = this->state.graph_fonts_reserved; n < io.Fonts->Fonts.Size; n++) {
                    if (ImGui::Selectable(io.Fonts->Fonts[n]->GetDebugName(), (io.Fonts->Fonts[n] == font_current))) {
                        io.FontDefault = io.Fonts->Fonts[n];
                        // Saving font to window configuration (Remove font size from font name)
                        this->state.font_file_name = std::string(io.FontDefault->GetDebugName());
                        auto sep_index = this->state.font_file_name.find(',');
                        this->state.font_file_name = this->state.font_file_name.substr(0, sep_index);
                        this->state.font_size = static_cast<int>(io.FontDefault->FontSize);
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Load Font from File");
            std::string help("Same font can be loaded multiple times with different font size.");
            this->tooltip.Marker(help);

            std::string label("Font Size");
            ImGui::InputInt(label.c_str(), &this->state.font_size, 1, 10, ImGuiInputTextFlags_None);
            // Validate font size
            if (this->state.font_size <= 5) {
                this->state.font_size = 5; // minimum valid font size
            }

            ImGui::BeginGroup();
            float widget_width = ImGui::CalcItemWidth() - (ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.x);
            ImGui::PushItemWidth(widget_width);
            this->file_browser.Button(
                this->state.font_file_name, megamol::gui::FileBrowserWidget::FileBrowserFlag::LOAD, "ttf");
            ImGui::SameLine();
            gui_utils::Utf8Encode(this->state.font_file_name);
            ImGui::InputText("Font Filename (.ttf)", &this->state.font_file_name, ImGuiInputTextFlags_None);
            gui_utils::Utf8Decode(this->state.font_file_name);
            ImGui::PopItemWidth();
            // Validate font file before offering load button
            bool valid_file = megamol::core::utility::FileUtils::FileWithExtensionExists<std::string>(
                this->state.font_file_name, std::string("ttf"));
            if (!valid_file) {
                megamol::gui::gui_utils::ReadOnlyWigetStyle(true);
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            }
            if (ImGui::Button("Add Font")) {
                this->state.font_apply = true;
            }
            if (!valid_file) {
                ImGui::PopItemFlag();
                megamol::gui::gui_utils::ReadOnlyWigetStyle(false);
                ImGui::SameLine();
                ImGui::TextColored(GUI_COLOR_TEXT_ERROR, "Please enter valid font file name.");
            }
            ImGui::EndGroup();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scale")) {
            float scale = megamol::gui::gui_scaling.Get();
            if (ImGui::RadioButton("100%", (scale == 1.0f))) {
                this->SetScale(1.0f);
            }
            if (ImGui::RadioButton("150%", (scale == 1.5f))) {
                this->SetScale(1.5f);
            }
            if (ImGui::RadioButton("200%", (scale == 2.0f))) {
                this->SetScale(2.0f);
            }
            if (ImGui::RadioButton("250%", (scale == 2.5f))) {
                this->SetScale(2.5f);
            }
            if (ImGui::RadioButton("300%", (scale == 3.0f))) {
                this->SetScale(3.0f);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
    ImGui::Separator();

    // HELP -------------------------------------------------------------------
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {
            this->state.open_popup_about = true;
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();

    ImGui::EndMainMenuBar();
}


void megamol::gui::GUIManager::draw_popups() {

    // Draw pop-ups defined in windows
    this->win_collection.PopUps();

    // Externally registered pop-ups
    auto popup_flags =
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;
    for (auto& popup_map : this->popup_collection) {
        if ((*popup_map.second.first)) {
            ImGui::OpenPopup(popup_map.first.c_str());
            (*popup_map.second.first) = false;
        }
        if (ImGui::BeginPopupModal(popup_map.first.c_str(), nullptr, popup_flags)) {
            popup_map.second.second();
            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Externally registered notifications
    for (auto& popup_map : this->notification_collection) {
        if (!std::get<1>(popup_map.second) && (*std::get<0>(popup_map.second))) {
            ImGui::OpenPopup(popup_map.first.c_str());
            (*std::get<0>(popup_map.second)) = false;
            // Mirror message in console log with info level
            megamol::core::utility::log::Log::DefaultLog.WriteInfo(std::get<2>(popup_map.second).c_str());
        }
        if (ImGui::BeginPopupModal(popup_map.first.c_str(), nullptr, popup_flags)) {
            ImGui::TextUnformatted(std::get<2>(popup_map.second).c_str());
            bool close = false;
            if (ImGui::Button("Ok")) {
                close = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Ok - Disable further notifications.")) {
                close = true;
                // Disable further notifications
                std::get<1>(popup_map.second) = true;
            }
            if (close || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // ABOUT
    if (this->state.open_popup_about) {
        this->state.open_popup_about = false;
        ImGui::OpenPopup("About");
    }
    bool open = true;
    if (ImGui::BeginPopupModal("About", &open, (ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))) {

        const std::string email("megamol@visus.uni-stuttgart.de");
        const std::string web_link("https://megamol.org/");
        const std::string github_link("https://github.com/UniStuttgart-VISUS/megamol");
        const std::string docu_link("https://github.com/UniStuttgart-VISUS/megamol/tree/master/plugins/gui");
        const std::string imgui_link("https://github.com/ocornut/imgui");

        const std::string mmstr = std::string("MegaMol - Version ") + std::to_string(MEGAMOL_CORE_MAJOR_VER) + (".") +
                                  std::to_string(MEGAMOL_CORE_MINOR_VER) + ("\ngit# ") +
                                  std::string(MEGAMOL_CORE_COMP_REV) + ("\n");
        const std::string mailstr = std::string("Contact: ") + email;
        const std::string webstr = std::string("Web: ") + web_link;
        const std::string gitstr = std::string("Git-Hub: ") + github_link;
        const std::string imguistr = ("Dear ImGui - Version ") + std::string(IMGUI_VERSION) + ("\n");
        const std::string imguigitstr = std::string("Git-Hub: ") + imgui_link;
        const std::string about = "Copyright (C) 2009-2020 by University of Stuttgart (VISUS).\nAll rights reserved.";

        ImGui::TextUnformatted(mmstr.c_str());

        if (ImGui::Button("Copy E-Mail")) {
            ImGui::SetClipboardText(email.c_str());
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(mailstr.c_str());

        if (ImGui::Button("Copy Website")) {
            ImGui::SetClipboardText(web_link.c_str());
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(webstr.c_str());

        if (ImGui::Button("Copy GitHub###megamol_copy_github")) {
            ImGui::SetClipboardText(github_link.c_str());
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(gitstr.c_str());

        ImGui::Separator();
        ImGui::TextUnformatted(imguistr.c_str());
        if (ImGui::Button("Copy GitHub###imgui_copy_github")) {
            ImGui::SetClipboardText(imgui_link.c_str());
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(imguigitstr.c_str());

        ImGui::Separator();
        ImGui::TextUnformatted(about.c_str());

        ImGui::Separator();
        if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Save project pop-up
    this->state.open_popup_save |= this->hotkeys[GUIManager::GuiHotkeyIndex::SAVE_PROJECT].is_pressed;
    bool confirmed, aborted;
    bool popup_failed = false;
    std::string filename;
    GraphPtr_t graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph();
    if (graph_ptr != nullptr) {
        filename = graph_ptr->GetFilename();
        vislib::math::Ternary save_gui_state(
            vislib::math::Ternary::TRI_FALSE); // Default for option asking for saving gui state
        this->state.open_popup_save |= this->configurator_ptr->ConsumeTriggeredGlobalProjectSave();

        if (this->file_browser.PopUp(filename, FileBrowserWidget::FileBrowserFlag::SAVE, "Save Project",
                this->state.open_popup_save, "lua", save_gui_state)) {

            std::string gui_state;
            if (save_gui_state.IsTrue()) {
                gui_state = this->project_to_lua_string(true);
            }

            popup_failed |=
                !this->configurator_ptr->GetGraphCollection().SaveProjectToFile(graph_ptr->UID(), filename, gui_state);
        }
        PopUps::Minimal(
            "Failed to Save Project", popup_failed, "See console log output for more information.", "Cancel");
    }
    this->state.open_popup_save = false;
    this->hotkeys[GUIManager::GuiHotkeyIndex::SAVE_PROJECT].is_pressed = false;

    // Load project pop-up
    popup_failed = false;
    if (graph_ptr != nullptr) {
        this->state.open_popup_load |= this->hotkeys[GUIManager::GuiHotkeyIndex::LOAD_PROJECT].is_pressed;
        if (this->file_browser.PopUp(filename, FileBrowserWidget::FileBrowserFlag::LOAD, "Load Project",
                this->state.open_popup_load, "lua")) {
            // Redirect project loading request to Lua_Wrapper_service and load new project to megamol graph
            /// GUI graph and GUI state are updated at next synchronization
            this->state.request_load_projet_file = filename;
        }
        PopUps::Minimal(
            "Failed to Load Project", popup_failed, "See console log output for more information.", "Cancel");
    }
    this->state.open_popup_load = false;
    this->hotkeys[GUIManager::GuiHotkeyIndex::LOAD_PROJECT].is_pressed = false;

    // File name for screenshot pop-up
    if (this->file_browser.PopUp(this->state.screenshot_filepath, FileBrowserWidget::FileBrowserFlag::SAVE,
            "Select Filename for Screenshot", this->state.open_popup_screenshot, "png")) {
        this->state.screenshot_filepath_id = 0;
    }
    this->state.open_popup_screenshot = false;
}


void GUIManager::check_multiple_hotkey_assignment() {
    if (this->state.hotkeys_check_once) {

        std::list<core::view::KeyCode> hotkeylist;
        hotkeylist.clear();

        // Fill with camera hotkeys for which no button parameters exist
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_W));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_A));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_S));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_D));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_C));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_V));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_Q));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_E));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_UP));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_DOWN));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_LEFT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_RIGHT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_W, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_A, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_S, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_D, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_C, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_V, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_Q, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_E, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_UP, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_DOWN, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_LEFT, core::view::Modifier::ALT));
        hotkeylist.emplace_back(core::view::KeyCode(core::view::Key::KEY_RIGHT, core::view::Modifier::ALT));

        // Add hotkeys of gui
        for (auto& h : this->hotkeys) {
            hotkeylist.emplace_back(h.keycode);
        }

        // Add hotkeys of configurator
        for (auto& h : this->configurator_ptr->GetHotkeys()) {
            hotkeylist.emplace_back(h.keycode);
        }

        if (auto graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph()) {
            for (auto& module_ptr : graph_ptr->Modules()) {
                for (auto& param : module_ptr->Parameters()) {

                    if (param.Type() == ParamType_t::BUTTON) {
                        auto keyCode = param.GetStorage<megamol::core::view::KeyCode>();
                        // Ignore not set hotekey
                        if (keyCode.key == core::view::Key::KEY_UNKNOWN) {
                            break;
                        }
                        // Check in hotkey map
                        bool found = false;
                        for (auto& kc : hotkeylist) {
                            if ((kc.key == keyCode.key) && (kc.mods.equals(keyCode.mods))) {
                                found = true;
                            }
                        }
                        if (!found) {
                            hotkeylist.emplace_back(keyCode);
                        } else {
                            megamol::core::utility::log::Log::DefaultLog.WriteWarn(
                                "[GUI] The hotkey [%s] of the parameter \"%s\" has already been assigned. "
                                ">>> If this hotkey is pressed, there will be no effect on this parameter!",
                                keyCode.ToString().c_str(), param.FullNameProject().c_str());
                        }
                    }
                }
            }
        }

        this->state.hotkeys_check_once = false;
    }
}


bool megamol::gui::GUIManager::is_hotkey_pressed(megamol::core::view::KeyCode keycode) {

    ImGuiIO& io = ImGui::GetIO();
    return (ImGui::IsKeyDown(static_cast<int>(keycode.key))) &&
           (keycode.mods.test(core::view::Modifier::ALT) == io.KeyAlt) &&
           (keycode.mods.test(core::view::Modifier::CTRL) == io.KeyCtrl) &&
           (keycode.mods.test(core::view::Modifier::SHIFT) == io.KeyShift);
}


void megamol::gui::GUIManager::load_preset_window_docking(ImGuiID global_docking_id) {

/// DOCKING
#ifdef IMGUI_HAS_DOCK

    // Create preset using DockBuilder
    /// https://github.com/ocornut/imgui/issues/2109#issuecomment-426204357
    //   -------------------------------
    //   |      |                      |
    //   | prop |       main           |
    //   |      |                      |
    //   |      |                      |
    //   |______|______________________|
    //   |           bottom            |
    //   -------------------------------

    ImGuiIO& io = ImGui::GetIO();
    auto dockspace_size = io.DisplaySize;
    ImGui::DockBuilderRemoveNode(global_docking_id);                            // Clear out existing layout
    ImGui::DockBuilderAddNode(global_docking_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
    ImGui::DockBuilderSetNodeSize(global_docking_id, dockspace_size);
    // Define new dock spaces
    ImGuiID dock_id_main = global_docking_id; // This variable will track the document node, however we are not using it
                                              // here as we aren't docking anything into it.
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.25f, nullptr, &dock_id_main);
    ImGuiID dock_id_prop = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.25f, nullptr, &dock_id_main);

    const auto func = [&, this](WindowConfiguration& wc) {
        switch (wc.WindowID()) {
        case (WindowConfiguration::WINDOW_ID_MAIN_PARAMETERS): {
            ImGui::DockBuilderDockWindow(wc.FullWindowTitle().c_str(), dock_id_prop);
        } break;
        case (WindowConfiguration::WINDOW_ID_TRANSFER_FUNCTION): {
            ImGui::DockBuilderDockWindow(wc.FullWindowTitle().c_str(), dock_id_prop);
        } break;
        case (WindowConfiguration::WINDOW_ID_CONFIGURATOR): {
            ImGui::DockBuilderDockWindow(wc.FullWindowTitle().c_str(), dock_id_main);
        } break;
        case (WindowConfiguration::WINDOW_ID_LOGCONSOLE): {
            ImGui::DockBuilderDockWindow(wc.FullWindowTitle().c_str(), dock_id_bottom);
        } break;
        default:
            break;
        }
    };
    this->win_collection.EnumWindows(func);

    ImGui::DockBuilderFinish(global_docking_id);
#endif
}


void megamol::gui::GUIManager::load_imgui_settings_from_string(const std::string& imgui_settings) {

/// DOCKING
#ifdef IMGUI_HAS_DOCK
    this->state.load_docking_preset = true;
    if (!imgui_settings.empty()) {
        ImGui::LoadIniSettingsFromMemory(imgui_settings.c_str(), imgui_settings.size());
        this->state.load_docking_preset = false;
    }
#endif
}


std::string megamol::gui::GUIManager::save_imgui_settings_to_string() {

/// DOCKING
#ifdef IMGUI_HAS_DOCK
    size_t buffer_size = 0;
    const char* buffer = ImGui::SaveIniSettingsToMemory(&buffer_size);
    if (buffer == nullptr) {
        return std::string();
    }
    std::string imgui_settings(buffer, buffer_size);
    return imgui_settings;
#else
    return std::string();
#endif
}


std::string megamol::gui::GUIManager::project_to_lua_string(bool as_lua) {

    std::string gui_state;
    if (this->state_to_string(gui_state)) {
        std::string return_state_str;

        if (as_lua) {
            return_state_str += std::string(GUI_START_TAG_SET_GUI_VISIBILITY) +
                                ((this->state.gui_visible) ? ("true") : ("false")) +
                                std::string(GUI_END_TAG_SET_GUI_VISIBILITY) + "\n";

            return_state_str += std::string(GUI_START_TAG_SET_GUI_SCALE) +
                                std::to_string(megamol::gui::gui_scaling.Get()) +
                                std::string(GUI_END_TAG_SET_GUI_SCALE) + "\n";

            return_state_str +=
                std::string(GUI_START_TAG_SET_GUI_STATE) + gui_state + std::string(GUI_END_TAG_SET_GUI_STATE) + "\n";
        } else {
            return_state_str += gui_state;
        }

        return return_state_str;
    }
    return std::string();
}


bool megamol::gui::GUIManager::state_from_string(const std::string& state) {

    try {
        nlohmann::json state_json = nlohmann::json::parse(state);
        if (!state_json.is_object()) {
            megamol::core::utility::log::Log::DefaultLog.WriteError(
                "[GUI] Invalid JSON object. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }

        // Read GUI state
        for (auto& header_item : state_json.items()) {
            if (header_item.key() == GUI_JSON_TAG_GUI) {
                auto gui_state = header_item.value();
                megamol::core::utility::get_json_value<bool>(gui_state, {"menu_visible"}, &this->state.menu_visible);
                int style = 0;
                megamol::core::utility::get_json_value<int>(gui_state, {"style"}, &style);
                this->state.style = static_cast<GUIManager::Styles>(style);
                this->state.style_changed = true;
                megamol::core::utility::get_json_value<std::string>(
                    gui_state, {"font_file_name"}, &this->state.font_file_name);
                megamol::core::utility::get_json_value<int>(gui_state, {"font_size"}, &this->state.font_size);
                this->state.font_apply = true;
                std::string imgui_settings;
                megamol::core::utility::get_json_value<std::string>(gui_state, {"imgui_settings"}, &imgui_settings);
                this->load_imgui_settings_from_string(imgui_settings);
            }
        }

        // Read window configurations
        this->win_collection.StateFromJSON(state_json);

        // Read configurator and graph state
        this->configurator_ptr->StateFromJSON(state_json);

        // Read GUI state of parameters (groups) of running graph
        if (auto graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph()) {
            for (auto& module_ptr : graph_ptr->Modules()) {
                std::string module_full_name = module_ptr->FullName();
                // Parameter Groups
                module_ptr->GUIParameterGroups().StateFromJSON(state_json, module_full_name);
                // Parameters
                for (auto& param : module_ptr->Parameters()) {
                    param.StateFromJSON(state_json, param.FullNameProject());
                    param.ForceSetGUIStateDirty();
                }
            }
        }

#ifdef GUI_VERBOSE
        megamol::core::utility::log::Log::DefaultLog.WriteInfo("[GUI] Read GUI state from JSON.");
#endif // GUI_VERBOSE
    } catch (...) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] JSON Error - Unable to read state from JSON. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


bool megamol::gui::GUIManager::state_to_string(std::string& out_state) {

    try {
        out_state.clear();
        nlohmann::json json_state;

        // Write GUI state
        json_state[GUI_JSON_TAG_GUI]["menu_visible"] = this->state.menu_visible;
        json_state[GUI_JSON_TAG_GUI]["style"] = static_cast<int>(this->state.style);
        gui_utils::Utf8Encode(this->state.font_file_name);
        json_state[GUI_JSON_TAG_GUI]["font_file_name"] = this->state.font_file_name;
        gui_utils::Utf8Decode(this->state.font_file_name);
        json_state[GUI_JSON_TAG_GUI]["font_size"] = this->state.font_size;
        json_state[GUI_JSON_TAG_GUI]["imgui_settings"] = this->save_imgui_settings_to_string();

        // Write window configuration
        this->win_collection.StateToJSON(json_state);

        // Write the configurator and graph state
        this->configurator_ptr->StateToJSON(json_state);

        // Write GUI state of parameters (groups) of running graph
        if (auto graph_ptr = this->configurator_ptr->GetGraphCollection().GetRunningGraph()) {
            for (auto& module_ptr : graph_ptr->Modules()) {
                std::string module_full_name = module_ptr->FullName();
                // Parameter Groups
                module_ptr->GUIParameterGroups().StateToJSON(json_state, module_full_name);
                // Parameters
                for (auto& param : module_ptr->Parameters()) {
                    param.StateToJSON(json_state, param.FullNameProject());
                }
            }
        }

        out_state = json_state.dump();
#ifdef GUI_VERBOSE
        megamol::core::utility::log::Log::DefaultLog.WriteInfo("[GUI] Wrote GUI state to JSON.");
#endif // GUI_VERBOSE

    } catch (...) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "[GUI] JSON Error - Unable to write state to JSON. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}



bool megamol::gui::GUIManager::create_not_existing_png_filepath(std::string& inout_filepath) {

    // Check for existing file
    bool created_filepath = false;
    if (!inout_filepath.empty()) {
        while (megamol::core::utility::FileUtils::FileExists<std::string>(inout_filepath)) {
            // Create new filename with iterating suffix
            std::string filename = megamol::core::utility::FileUtils::GetFilenameStem<std::string>(inout_filepath);
            std::string id_separator = "_";
            bool new_separator = false;
            auto separator_index = filename.find_last_of(id_separator);
            if (separator_index != std::string::npos) {
                auto last_id_str = filename.substr(separator_index + 1);
                try {
                    this->state.screenshot_filepath_id = std::stoi(last_id_str);
                } catch (...) { new_separator = true; }
                this->state.screenshot_filepath_id++;
                if (new_separator) {
                    this->state.screenshot_filepath =
                        filename + id_separator + std::to_string(this->state.screenshot_filepath_id) + ".png";
                } else {
                    inout_filepath = filename.substr(0, separator_index + 1) +
                                     std::to_string(this->state.screenshot_filepath_id) + ".png";
                }
            } else {
                inout_filepath = filename + id_separator + std::to_string(this->state.screenshot_filepath_id) + ".png";
            }
        }
        created_filepath = true;
    }
    return created_filepath;
}


void GUIManager::RegisterWindow(
    const std::string& window_name, std::function<void(WindowConfiguration::Basic&)> const& callback) {

    // Create new window configuration (might be already created via GUI state)
    if (window_name.empty()) {
        megamol::core::utility::log::Log::DefaultLog.WriteWarn(
            "[GUI] Invalid window name. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return;
    }
    auto hash_id = std::hash<std::string>()(window_name);
    if (!this->win_collection.WindowConfigurationExists(hash_id)) {
        WindowConfiguration wc_tmp(
            window_name, const_cast<std::function<void(WindowConfiguration::Basic&)>&>(callback));
        wc_tmp.Config().show = true;
        wc_tmp.Config().flags = ImGuiWindowFlags_AlwaysAutoResize;
        this->win_collection.AddWindowConfiguration(wc_tmp);
    }
    // Set volatile window callback function for existing window configuration
    const auto func = [&, this](WindowConfiguration& wc) {
        if (wc.Hash() == hash_id) {
            wc.SetVolatileCallback(const_cast<std::function<void(WindowConfiguration::Basic&)>&>(callback));
        }
    };
    this->win_collection.EnumWindows(func);
}


void GUIManager::RegisterPopUp(const std::string& name, bool& open, const std::function<void()> &callback) {

    this->popup_collection[name] = std::pair<bool*, std::function<void()>>(&open, const_cast<std::function<void()>&>(callback));
}


void GUIManager::RegisterNotification(const std::string &name, bool &open, const std::string &message) {

    this->notification_collection[name] = std::tuple<bool*, bool, std::string>(&open, false, message);
}

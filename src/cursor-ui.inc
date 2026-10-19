// Cursor target is shared between the show-event thread and XAML thread.
// The stable identity is resolved once, never silently reassigned after hotplug.
std::mutex g_targetMutex;
cursor_brightness::PanelSelection g_selection;
std::atomic<unsigned> g_targetGeneration{0};
std::atomic<bool> g_enableBrightnessKeys{true};
std::atomic<bool> g_diagnosticKeys{false};
std::atomic<int> g_brightnessStep{5};
std::atomic<bool> g_standaloneBridge{false};

std::wstring PanelDisplaySignature(const brightness::Display& display) {
    return display.stableId + L":" + std::to_wstring(static_cast<int>(display.transport)) +
        (display.percent >= 0 ? L":ready" : L":unavailable");
}

void LatchCursorTarget() {
    std::lock_guard<std::mutex> lock(g_targetMutex);
    POINT point{};
    if (GetCursorPos(&point) && g_selection.Open(point.x, point.y)) ++g_targetGeneration;
}
void CloseCursorTarget() {
    std::lock_guard<std::mutex> lock(g_targetMutex);
    g_selection.Close();
}
std::wstring SelectedTarget(const std::vector<brightness::Display>& displays) {
    std::lock_guard<std::mutex> lock(g_targetMutex);
    std::vector<cursor_brightness::Candidate> candidates;
    for (const auto& d : displays) candidates.push_back({d.stableId, {d.rect.left, d.rect.top, d.rect.right, d.rect.bottom}});
    return g_selection.Resolve(candidates);
}

void PopulateSliderPanel(wuxc::StackPanel const& panel, Injection& injection) {
    const auto displays = g_engine->GetDisplays();
    injection.builtFor.clear();
    for (const auto& d : displays) injection.builtFor.push_back(PanelDisplaySignature(d));
    injection.targetGeneration = g_targetGeneration.load();
    panel.Visibility(wux::Visibility::Visible);
    if (displays.empty()) {
        wuxc::TextBlock note;
        note.Text(L"Detecting displays…");
        note.FontSize(11);
        note.Opacity(0.7);
        panel.Children().Append(note);
        return;
    }
    // Fixed-height rows keep exactly two monitors in view, including when a
    // monitor is unavailable. Additional rows stay reachable by wheel/touch/keys.
    const double rowHeight = std::max(40.0, g_metrics.rowHeight) + 32.0;
    wuxc::StackPanel rows;
    rows.Orientation(wuxc::Orientation::Vertical);
    wuxc::ScrollViewer viewport;
    viewport.Name(L"CursorBrightnessMonitorList");
    viewport.VerticalScrollMode(wuxc::ScrollMode::Enabled);
    viewport.VerticalScrollBarVisibility(wuxc::ScrollBarVisibility::Auto);
    viewport.HorizontalScrollMode(wuxc::ScrollMode::Disabled);
    viewport.HorizontalScrollBarVisibility(wuxc::ScrollBarVisibility::Disabled);
    viewport.IsVerticalRailEnabled(true);
    viewport.HorizontalContentAlignment(wux::HorizontalAlignment::Stretch);
    viewport.MaxHeight(rowHeight * 2);
    viewport.Content(rows);
    panel.Children().Append(viewport);
    for (const auto& d : displays) {
        wuxc::StackPanel monitorRow;
        monitorRow.Height(rowHeight);
        auto rowTitle = std::make_shared<RowTitle>();
        rowTitle->name = d.name;
        if (d.transport == brightness::Transport::HdrSdr) rowTitle->name += L" · SDR brightness";
        rowTitle->text = MakeRowTitle(winrt::hstring{rowTitle->name});
        rowTitle->text.TextTrimming(wux::TextTrimming::CharacterEllipsis);
        monitorRow.Children().Append(rowTitle->text);
        rows.Children().Append(monitorRow);
        const bool available = d.transport != brightness::Transport::None && d.percent >= 0;
        if (!available) {
            wuxc::TextBlock note;
            note.Text(L"Brightness unavailable"); note.FontSize(11); note.Opacity(0.7);
            monitorRow.Children().Append(note);
            continue;
        }
        const double step = d.transport == brightness::Transport::DdcCi && d.vcpMax > 0 ? std::max(1.0, 100.0 / d.vcpMax) : 1.0;
        wuxc::Slider slider = MakeSlider(d.percent, step);
        wux::FrameworkElement icon = MakeBrightnessIcon();
        SetIconLevel(icon, d.percent);
        monitorRow.Children().Append(MakeSliderRow(MakeIconHost(icon, slider, injection), slider, nullptr, 0));
        // Let the containing ScrollViewer own the wheel when there are more than
        // two rows, so scrolling over a slider never changes the wrong monitor.
        if (displays.size() <= 2) AttachWheel(slider, injection);
        else injection.wheelRevokers.push_back(slider.PointerWheelChanged(winrt::auto_revoke,
            [weak = winrt::make_weak(viewport)](wf::IInspectable const&, wux::Input::PointerRoutedEventArgs const& args) {
                if (auto view = weak.get()) {
                    const auto properties = args.GetCurrentPoint(view).Properties();
                    if (!properties.IsHorizontalMouseWheel() && properties.MouseWheelDelta()) {
                        view.ChangeView(nullptr, winrt::box_value(std::clamp(view.VerticalOffset() - properties.MouseWheelDelta() * 0.5, 0.0, view.ScrollableHeight())).as<wf::IReference<double>>(), nullptr, true);
                        args.Handled(true);
                    }
                }
            }));
        rowTitle->sliders.push_back({winrt::make_weak(slider), false});
        injection.revokers.push_back(slider.ValueChanged(winrt::auto_revoke,
            [id = d.stableId, icon, rowTitle](wf::IInspectable const&, wuxc::Primitives::RangeBaseValueChangedEventArgs const& args) {
                SetIconLevel(icon, args.NewValue());
                UpdateRowTitle(*rowTitle);
                if (!g_suppressValueChanged && g_engine) {
                    const int percent = static_cast<int>(std::lround(args.NewValue()));
                    if (g_standaloneBridge.load()) {
                        if (!cursor_brightness::bridge::SetPercent(id, percent)) {
                            Wh_Log(L"Standalone brightness app unavailable; request was not redirected");
                            g_engine->RequestRefresh();
                        }
                    } else g_engine->SetPercent(id, percent);
                }
            }));
        Injection::Binding binding{d.stableId, d.name, {}, {}, rowTitle};
        binding.slider = winrt::make_weak(slider);
        binding.icon = winrt::make_weak(icon);
        injection.bindings.push_back(std::move(binding));
        UpdateRowTitle(*rowTitle);
    }
}

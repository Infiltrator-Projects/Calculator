// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

private extension Color {
    init(hex: UInt32) {
        self.init(
            red: Double((hex >> 16) & 0xff) / 255.0,
            green: Double((hex >> 8) & 0xff) / 255.0,
            blue: Double(hex & 0xff) / 255.0
        )
    }
}

private enum ThemePreference: String, CaseIterable, Identifiable {
    case system
    case day
    case night

    var id: String { rawValue }

    var label: String {
        switch self {
        case .system: return "Follow System"
        case .day: return "Day"
        case .night: return "Night"
        }
    }

    var preferredScheme: ColorScheme? {
        switch self {
        case .system: return nil
        case .day: return .light
        case .night: return .dark
        }
    }
}

// SwiftUI resolves native Color values from Common through the bridge. The key
// set mirrors the shared semantic palette; this layer must not invent alternate
// Day/Night colour constants.
private struct CalculatorPalette {
    let background: Color
    let panel: Color
    let card: Color
    let surface: Color
    let input: Color
    let border: Color
    let text: Color
    let title: Color
    let muted: Color
    let subtle: Color
    let primary: Color
    let primaryText: Color
    let selected: Color
    let selectedText: Color
    let neutralAccent: Color
    let success: Color
    let warning: Color
    let fault: Color
    let info: Color
    let operation: Color
    let cardHover: Color
    let surfaceHover: Color
    let operationHover: Color
    let equalsHover: Color
    let heading: Color
    let summary: Color
    let kicker: Color
    let detailLabel: Color
    let note: Color
    let statusBorder: Color
    let accentForeground: Color
    let accentHover: Color
    let selectedSummary: Color
    let warningMuted: Color
    let warningBorder: Color
    let successBorder: Color

    init(dark: Bool) {
        let common = CalculatorBridge.themePalette(dark: dark)

        func colour(_ key: String) -> Color {
            Color(hex: common[key]?.uint32Value ?? 0)
        }

        background = colour("background")
        panel = colour("panel")
        card = colour("card")
        surface = colour("surface")
        input = colour("input")
        border = colour("border")
        text = colour("text")
        title = colour("title")
        muted = colour("muted")
        subtle = colour("subtle")
        primary = colour("buttonBackground")
        primaryText = colour("buttonForeground")
        selected = colour("selectionBackground")
        selectedText = colour("selectionForeground")
        neutralAccent = colour("neutralAccent")
        success = colour("success")
        warning = colour("warning")
        fault = colour("fault")
        info = colour("info")
        operation = colour("operation")
        cardHover = colour("cardHover")
        surfaceHover = colour("surfaceHover")
        operationHover = colour("operationHover")
        equalsHover = colour("equalsHover")
        heading = colour("heading")
        summary = colour("summary")
        kicker = colour("kicker")
        detailLabel = colour("detailLabel")
        note = colour("note")
        statusBorder = colour("statusBorder")
        accentForeground = colour("accentForeground")
        accentHover = colour("accentHover")
        selectedSummary = colour("selectedSummary")
        warningMuted = colour("warningMuted")
        warningBorder = colour("warningBorder")
        successBorder = colour("successBorder")
    }

}

// Geometry values that are product-family design semantics come from Common;
 // touch-native SwiftUI composition remains local to iPhone.
private struct SharedDesignMetrics {
    let smallRadius: CGFloat
    let controlRadius: CGFloat
    let cardRadius: CGFloat
    let panelRadius: CGFloat
    let compactSpacing: CGFloat
    let controlSpacing: CGFloat
    let sectionSpacing: CGFloat
    let contentPadding: CGFloat
    let screenPadding: CGFloat

    init() {
        let common = CalculatorBridge.designMetrics()
        func value(_ key: String) -> CGFloat {
            CGFloat(common[key]?.doubleValue ?? 0)
        }

        smallRadius = value("smallRadius")
        controlRadius = value("controlRadius")
        cardRadius = value("cardRadius")
        panelRadius = value("panelRadius")
        compactSpacing = value("compactSpacing")
        controlSpacing = value("controlSpacing")
        sectionSpacing = value("sectionSpacing")
        contentPadding = value("contentPadding")
        screenPadding = value("screenPadding")
    }
}

private let sharedDesign = SharedDesignMetrics()

struct ContentView: View {
    @StateObject private var model = CalculatorModel()
    @Environment(\.colorScheme) private var systemColorScheme
    @AppStorage("themePreference") private var themePreferenceRaw = ThemePreference.system.rawValue

    private var themePreference: ThemePreference {
        get { ThemePreference(rawValue: themePreferenceRaw) ?? .system }
        nonmutating set { themePreferenceRaw = newValue.rawValue }
    }

    private var palette: CalculatorPalette {
        switch themePreference {
        case .day: return CalculatorPalette(dark: false)
        case .night: return CalculatorPalette(dark: true)
        case .system:
            return CalculatorPalette(dark: systemColorScheme == .dark)
        }
    }

    var body: some View {
        ZStack {
            palette.background.ignoresSafeArea()

            VStack(spacing: 14) {
                header
                modeStrip
                displayCard
                keypad
                footer
            }
            .padding(sharedDesign.screenPadding)
        }
        .preferredColorScheme(themePreference.preferredScheme)
        .sheet(isPresented: $model.showingHistory) {
            HistoryView(model: model)
        }
        .sheet(isPresented: $model.showingTools) {
            AdvancedToolsView(model: model)
        }
        .sheet(isPresented: $model.showingResults) {
            AdditionalResultsView(model: model)
        }
        .sheet(isPresented: $model.showingBases) {
            ProgrammerRepresentationsView(model: model)
        }
    }

    private var header: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 1) {
                Text("Calculator")
                    .font(CalculatorTypography.display(29, relativeTo: .title))
                    .foregroundStyle(palette.heading)

                Text("PRECISION CALCULATOR")
                    .font(CalculatorTypography.bold(9, relativeTo: .caption2))
                    .tracking(1.2)
                    .foregroundStyle(palette.kicker)
            }

            Spacer()

            Menu {
                Picker("Theme", selection: Binding(
                    get: { themePreference },
                    set: { themePreference = $0 }
                )) {
                    ForEach(ThemePreference.allCases) { preference in
                        Text(preference.label).tag(preference)
                    }
                }

                Picker("Scientific digits", selection: Binding(
                    get: { model.scientificDigits },
                    set: { model.setScientificDigits($0) }
                )) {
                    ForEach([16, 25, 50, 100, 250, 500, 1000], id: \.self) {
                        Text("\($0) digits").tag($0)
                    }
                }

                Picker("History", selection: Binding(
                    get: { model.historyLimit },
                    set: { model.setHistoryLimit($0) }
                )) {
                    Text("History: Unlimited").tag(0)
                    ForEach([100, 500, 1000], id: \.self) {
                        Text("History: \($0)").tag($0)
                    }
                }
            } label: {
                Image(systemName: "circle.lefthalf.filled")
                    .imageScale(.medium)
                    .frame(width: 42, height: 38)
            }
            .buttonStyle(CalculatorToolbarButtonStyle(palette: palette))
            .accessibilityLabel("Settings")

            if model.mode == .programmer {
                Button {
                    model.showingBases = true
                } label: {
                    Image(systemName: "number.square")
                        .imageScale(.medium)
                        .frame(width: 42, height: 38)
                }
                .buttonStyle(CalculatorToolbarButtonStyle(palette: palette))
                .accessibilityLabel("Programmer base representations")
            } else {
                Button {
                    model.showingResults = true
                } label: {
                    Image(systemName: "list.bullet.rectangle")
                        .imageScale(.medium)
                        .frame(width: 42, height: 38)
                }
                .buttonStyle(CalculatorToolbarButtonStyle(palette: palette))
                .accessibilityLabel("Additional result representations")
            }

            Button {
                model.showingTools = true
            } label: {
                Image(systemName: "wrench.and.screwdriver")
                    .imageScale(.medium)
                    .frame(width: 42, height: 38)
            }
            .buttonStyle(CalculatorToolbarButtonStyle(palette: palette))
            .accessibilityLabel("Advanced calculator tools")

            Button {
                model.showingHistory = true
            } label: {
                Image(systemName: "clock.arrow.circlepath")
                    .imageScale(.medium)
                    .frame(width: 42, height: 38)
            }
            .buttonStyle(CalculatorToolbarButtonStyle(palette: palette))
            .accessibilityLabel("Calculation history")
        }
    }

    private var modeStrip: some View {
        HStack(spacing: 5) {
            ForEach(CalcMode.allCases) { mode in
                Button(mode.rawValue) {
                    withAnimation(.easeOut(duration: 0.15)) {
                        model.selectMode(mode)
                    }
                }
                .font(CalculatorTypography.bold(12, relativeTo: .caption))
                .foregroundStyle(
                    model.mode == mode
                        ? palette.primaryText
                        : palette.kicker
                )
                .frame(maxWidth: .infinity, minHeight: 38)
                .background(
                    RoundedRectangle(cornerRadius: 8)
                        .fill(
                            model.mode == mode
                                ? palette.primary
                                : Color.clear
                        )
                )
            }
        }
        .padding(5)
        .background(
            RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                .fill(palette.surface)
                .overlay(
                    RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                        .stroke(palette.border, lineWidth: 1)
                )
        )
    }

    private var displayCard: some View {
        VStack(spacing: 8) {
            TextField(
                "Enter an expression or variable assignment",
                text: Binding(
                    get: { model.expression },
                    set: { model.setExpression($0) }
                )
            )
            .textInputAutocapitalization(.never)
            .autocorrectionDisabled()
            .multilineTextAlignment(.trailing)
            .font(CalculatorTypography.regular(15, relativeTo: .body))
            .foregroundStyle(palette.summary)
            .padding(.horizontal, 12)
            .frame(minHeight: 42)
            .background(
                RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                    .fill(palette.input)
                    .overlay(
                        RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                            .stroke(palette.border, lineWidth: 1)
                    )
            )
            .onSubmit { model.calculate() }

            Text(model.display)
                .font(CalculatorTypography.display(46, relativeTo: .largeTitle))
                .foregroundStyle(palette.heading)
                .lineLimit(1)
                .minimumScaleFactor(0.42)
                .textSelection(.enabled)
                .accessibilityLabel("Calculation result \(model.display)")
                .frame(maxWidth: .infinity, alignment: .trailing)
                .padding(.top, 4)

            Text(model.status)
                .font(CalculatorTypography.bold(9, relativeTo: .caption2))
                .tracking(0.9)
                .foregroundStyle(
                    model.fault
                        ? palette.fault
                        : palette.note
                )
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .frame(maxWidth: .infinity, alignment: .trailing)
        }
        .padding(sharedDesign.contentPadding)
        .background(
            RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                .fill(palette.panel)
                .overlay(
                    RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                        .stroke(palette.statusBorder, lineWidth: 1)
                )
        )
    }

    private var keypad: some View {
        ScrollView(showsIndicators: false) {
            VStack(spacing: sharedDesign.controlSpacing) {
                ForEach(Array(model.rows.enumerated()), id: \.offset) { _, row in
                    HStack(spacing: sharedDesign.controlSpacing) {
                        ForEach(Array(row.enumerated()), id: \.offset) { _, key in
                            CalculatorKey(
                                title: model.visibleTitle(key),
                                selected: model.keyIsSelected(key),
                                enabled: model.keyIsEnabled(key),
                                palette: palette
                            ) {
                                model.press(key)
                            }
                        }
                    }
                }
            }
            .padding(.vertical, 1)
        }
    }

    private var footer: some View {
        Text("Keyboard ready · Variables, memory and history retained")
            .font(CalculatorTypography.regular(9, relativeTo: .caption2))
            .foregroundStyle(palette.detailLabel)
            .frame(maxWidth: .infinity, alignment: .leading)
    }
}

private struct CalculatorKey: View {
    let title: String
    let selected: Bool
    let enabled: Bool
    let palette: CalculatorPalette
    let action: () -> Void

    private var kind: KeyKind {
        if title == "=" { return .equals }
        if ["C", "AC", "⌫"].contains(title) { return .clear }
        if [
            "MC", "MR", "MS", "M+", "M−", "DEG", "RAD", "GRAD",
            "2nd", "HYP", "F-E",
            "BIN", "OCT", "DEC", "HEX",
            "W8", "W16", "W32", "W64", "U/S"
        ].contains(title) { return .utility }

        if title.count == 1,
           ("0"..."9").contains(title) ||
           ["A", "B", "C", "D", "E", "F", "."].contains(title) {
            return .number
        }

        return .operation
    }

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(CalculatorTypography.bold(kind == .equals ? 19 : 16, relativeTo: .body))
                .frame(maxWidth: .infinity, minHeight: 54)
                .contentShape(Rectangle())
        }
        .foregroundStyle(foreground)
        .background(
            RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                .fill(background)
                .overlay(
                    RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                        .stroke(border, lineWidth: 1)
                )
        )
        .opacity(enabled ? 1.0 : 0.32)
        .disabled(!enabled)
        .accessibilityLabel(title)
        .accessibilityValue(Text(selected ? "Selected" : ""))
    }

    private var background: Color {
        if selected { return palette.selected }
        switch kind {
        case .number: return palette.card
        case .operation: return palette.operation
        case .utility: return palette.surface
        case .clear: return palette.card
        case .equals: return palette.primary
        }
    }

    private var foreground: Color {
        switch kind {
        case .clear: return palette.warningMuted
        case .equals: return palette.primaryText
        case .utility: return selected ? palette.selectedSummary : palette.muted
        case .number, .operation: return palette.text
        }
    }

    private var border: Color {
        if selected { return palette.accentHover }
        if kind == .clear { return palette.warningBorder }
        if kind == .equals { return palette.primary }
        return palette.border
    }

    private enum KeyKind {
        case number
        case operation
        case utility
        case clear
        case equals
    }
}

private struct CalculatorToolbarButtonStyle: ButtonStyle {
    let palette: CalculatorPalette

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .foregroundStyle(palette.muted)
            .background(
                RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                    .fill(
                        configuration.isPressed
                            ? palette.card
                            : palette.surface
                    )
                    .overlay(
                        RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                            .stroke(
                                configuration.isPressed
                                    ? palette.accentHover
                                    : palette.border,
                                lineWidth: 1
                            )
                    )
            )
    }
}


private struct AdvancedToolsView: View {
    @ObservedObject var model: CalculatorModel
    @Environment(\.dismiss) private var dismiss
    @Environment(\.colorScheme) private var systemColorScheme
    @AppStorage("themePreference") private var themePreferenceRaw = ThemePreference.system.rawValue

    @State private var descriptors: [CalculatorToolDescriptor] = []
    @State private var selected = 0
    @State private var input = ""
    @State private var output = ""
    @State private var points: [CalculatorGraphPoint] = []

    private var themePreference: ThemePreference {
        ThemePreference(rawValue: themePreferenceRaw) ?? .system
    }

    private var palette: CalculatorPalette {
        switch themePreference {
        case .day: return CalculatorPalette(dark: false)
        case .night: return CalculatorPalette(dark: true)
        case .system: return CalculatorPalette(dark: systemColorScheme == .dark)
        }
    }

    private var current: CalculatorToolDescriptor? {
        descriptors.first { $0.id == selected }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                palette.background.ignoresSafeArea()
                ScrollView {
                    VStack(alignment: .leading, spacing: sharedDesign.sectionSpacing) {
                        Picker("Tool", selection: $selected) {
                            ForEach(descriptors) { item in
                                Text(item.name).tag(item.id)
                            }
                        }
                        .pickerStyle(.menu)
                        .tint(palette.accentForeground)
                        .onChange(of: selected) { _, _ in
                            loadExample()
                        }

                        if let current {
                            Text(current.prompt)
                                .font(CalculatorTypography.regular(14, relativeTo: .body))
                                .foregroundStyle(palette.summary)
                            Text("Example: \(current.example)")
                                .font(CalculatorTypography.regular(12, relativeTo: .caption))
                                .foregroundStyle(palette.note)
                        }

                        TextField("Input", text: $input)
                            .font(CalculatorTypography.regular(15, relativeTo: .body))
                            .textInputAutocapitalization(.never)
                            .autocorrectionDisabled()
                            .padding(sharedDesign.contentPadding)
                            .foregroundStyle(palette.text)
                            .background(
                                RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                                    .fill(palette.input)
                                    .overlay(
                                        RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                                            .stroke(palette.border, lineWidth: 1)
                                    )
                            )
                            .onSubmit { run() }

                        Button("Run") { run() }
                            .font(CalculatorTypography.bold(15, relativeTo: .body))
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 11)
                            .foregroundStyle(palette.primaryText)
                            .background(
                                RoundedRectangle(cornerRadius: sharedDesign.controlRadius)
                                    .fill(palette.primary)
                            )

                        if !output.isEmpty {
                            Text(output)
                                .font(CalculatorTypography.regular(14, relativeTo: .body))
                                .foregroundStyle(palette.text)
                                .textSelection(.enabled)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(sharedDesign.contentPadding)
                                .background(
                                    RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                                        .fill(palette.panel)
                                        .overlay(
                                            RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                                                .stroke(palette.statusBorder, lineWidth: 1)
                                        )
                                )
                        }

                        if !points.isEmpty {
                            ToolGraphView(points: points, palette: palette)
                                .frame(height: 240)
                                .background(
                                    RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                                        .fill(palette.panel)
                                        .overlay(
                                            RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                                                .stroke(palette.statusBorder, lineWidth: 1)
                                        )
                                )
                        }
                    }
                    .padding(sharedDesign.screenPadding)
                }
            }
            .navigationTitle("Calculator Tools")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .preferredColorScheme(themePreference.preferredScheme)
        .onAppear {
            descriptors = model.advancedToolDescriptors()
            selected = descriptors.first?.id ?? 0
            loadExample()
        }
    }

    private func loadExample() {
        guard let current else { return }
        input = current.example
        output = ""
        points = []
    }

    private func run() {
        let result = model.evaluateAdvancedTool(index: selected, input: input)
        output = result.text
        points = result.points
    }
}

private struct ToolGraphView: View {
    let points: [CalculatorGraphPoint]
    let palette: CalculatorPalette

    var body: some View {
        GeometryReader { geometry in
            Canvas { context, size in
                let valid = points.filter { $0.valid }
                guard let first = valid.first else { return }
                var xmin = first.x
                var xmax = first.x
                var ymin = first.y
                var ymax = first.y
                for point in valid.dropFirst() {
                    xmin = min(xmin, point.x)
                    xmax = max(xmax, point.x)
                    ymin = min(ymin, point.y)
                    ymax = max(ymax, point.y)
                }
                if xmin == xmax { xmin -= 1; xmax += 1 }
                if ymin == ymax { ymin -= 1; ymax += 1 }

                let inset: CGFloat = 12
                let width = max(1, size.width - inset * 2)
                let height = max(1, size.height - inset * 2)
                func px(_ x: Double) -> CGFloat {
                    inset + CGFloat((x - xmin) / (xmax - xmin)) * width
                }
                func py(_ y: Double) -> CGFloat {
                    inset + CGFloat((ymax - y) / (ymax - ymin)) * height
                }

                var axes = Path()
                if xmin <= 0 && xmax >= 0 {
                    axes.move(to: CGPoint(x: px(0), y: inset))
                    axes.addLine(to: CGPoint(x: px(0), y: inset + height))
                }
                if ymin <= 0 && ymax >= 0 {
                    axes.move(to: CGPoint(x: inset, y: py(0)))
                    axes.addLine(to: CGPoint(x: inset + width, y: py(0)))
                }
                context.stroke(axes, with: .color(palette.statusBorder), lineWidth: 1)

                var path = Path()
                var drawing = false
                for point in points {
                    guard point.valid else {
                        drawing = false
                        continue
                    }
                    let position = CGPoint(x: px(point.x), y: py(point.y))
                    if drawing {
                        path.addLine(to: position)
                    } else {
                        path.move(to: position)
                        drawing = true
                    }
                }
                // Common's text role resolves light on Night and dark on Day,
                // so the plotted data remains visible against either panel.
                context.stroke(path, with: .color(palette.text), lineWidth: 2)
            }
        }
        .accessibilityLabel("Graph")
    }
}

private struct AdditionalResultsView: View {
    @ObservedObject var model: CalculatorModel
    @Environment(\.dismiss) private var dismiss
    @Environment(\.colorScheme) private var systemColorScheme
    @AppStorage("themePreference") private var themePreferenceRaw = ThemePreference.system.rawValue

    private var themePreference: ThemePreference {
        ThemePreference(rawValue: themePreferenceRaw) ?? .system
    }

    private var palette: CalculatorPalette {
        switch themePreference {
        case .day: return CalculatorPalette(dark: false)
        case .night: return CalculatorPalette(dark: true)
        case .system: return CalculatorPalette(dark: systemColorScheme == .dark)
        }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                palette.background.ignoresSafeArea()
                ScrollView(.horizontal, showsIndicators: true) {
                    Text(model.additionalResultsText())
                        .font(CalculatorTypography.regular(15, relativeTo: .body))
                        .foregroundStyle(palette.summary)
                        .textSelection(.enabled)
                        .padding(sharedDesign.sectionSpacing)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                .background(palette.panel)
            }
            .navigationTitle("Additional Results")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .preferredColorScheme(themePreference.preferredScheme)
    }
}

private struct ProgrammerRepresentationsView: View {
    @ObservedObject var model: CalculatorModel
    @Environment(\.dismiss) private var dismiss
    @Environment(\.colorScheme) private var systemColorScheme
    @AppStorage("themePreference") private var themePreferenceRaw = ThemePreference.system.rawValue

    private var themePreference: ThemePreference {
        ThemePreference(rawValue: themePreferenceRaw) ?? .system
    }

    private var palette: CalculatorPalette {
        switch themePreference {
        case .day: return CalculatorPalette(dark: false)
        case .night: return CalculatorPalette(dark: true)
        case .system: return CalculatorPalette(dark: systemColorScheme == .dark)
        }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                palette.background.ignoresSafeArea()
                ScrollView(.horizontal, showsIndicators: true) {
                    Text(model.programmerRepresentationsText())
                        .font(CalculatorTypography.regular(15, relativeTo: .body))
                        .foregroundStyle(palette.text)
                        .textSelection(.enabled)
                        .padding(sharedDesign.sectionSpacing)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                .background(palette.panel)
            }
            .navigationTitle("Programmer Representations")
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Swap Endian") {
                        _ = model.swapProgrammerEndianness()
                    }
                    .disabled(model.mode != .programmer)
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .preferredColorScheme(themePreference.preferredScheme)
    }
}

private struct HistoryView: View {
    @ObservedObject var model: CalculatorModel
    @Environment(\.dismiss) private var dismiss
    @Environment(\.colorScheme) private var systemColorScheme
    @AppStorage("themePreference") private var themePreferenceRaw = ThemePreference.system.rawValue
    @State private var entries: [CalculatorHistoryRow] = []

    private var themePreference: ThemePreference {
        ThemePreference(rawValue: themePreferenceRaw) ?? .system
    }

    private var palette: CalculatorPalette {
        switch themePreference {
        case .day: return CalculatorPalette(dark: false)
        case .night: return CalculatorPalette(dark: true)
        case .system: return CalculatorPalette(dark: systemColorScheme == .dark)
        }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                palette.background.ignoresSafeArea()

                if entries.isEmpty {
                    Text("No calculations yet.")
                        .font(CalculatorTypography.regular(15, relativeTo: .body))
                        .foregroundStyle(palette.summary)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    ScrollView {
                        LazyVStack(spacing: sharedDesign.controlSpacing) {
                            ForEach(entries) { entry in
                                HStack(spacing: sharedDesign.controlSpacing) {
                                    Button {
                                        model.recallHistory(entry.id)
                                        dismiss()
                                    } label: {
                                        VStack(alignment: .leading, spacing: 4) {
                                            Text(entry.input)
                                                .font(CalculatorTypography.regular(14, relativeTo: .body))
                                                .foregroundStyle(palette.text)
                                                .lineLimit(2)
                                            Text("= \(entry.output)")
                                                .font(CalculatorTypography.bold(15, relativeTo: .body))
                                                .foregroundStyle(entry.ok ? palette.title : palette.fault)
                                                .lineLimit(2)
                                        }
                                        .frame(maxWidth: .infinity, alignment: .leading)
                                        .padding(sharedDesign.contentPadding)
                                        .background(
                                            RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                                                .fill(palette.card)
                                                .overlay(
                                                    RoundedRectangle(cornerRadius: sharedDesign.cardRadius)
                                                        .stroke(palette.border, lineWidth: 1)
                                                )
                                        )
                                    }
                                    .buttonStyle(.plain)
                                    .accessibilityLabel(
                                        "Recall \(entry.input), result \(entry.output)")

                                    Button {
                                        if model.deleteHistory(entry.id) {
                                            entries = model.historyEntries()
                                        }
                                    } label: {
                                        Text("Delete")
                                            .font(CalculatorTypography.bold(
                                                13, relativeTo: .body))
                                            .foregroundStyle(palette.warning)
                                    }
                                    .buttonStyle(.plain)
                                    .accessibilityLabel(
                                        "Delete \(entry.input) from history")
                                }
                            }
                        }
                        .padding(sharedDesign.sectionSpacing)
                    }
                }
            }
            .navigationTitle("Calculation History")
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Clear") {
                        model.clearHistory()
                        entries = model.historyEntries()
                    }
                    .foregroundStyle(palette.warning)
                    .disabled(entries.isEmpty)
                }

                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .preferredColorScheme(themePreference.preferredScheme)
        .onAppear { entries = model.historyEntries() }
    }
}

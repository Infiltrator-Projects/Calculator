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
    }

}

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
    }

    private var header: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 1) {
                Text("Calculator")
                    .font(CalculatorTypography.display(29, relativeTo: .title))
                    .foregroundStyle(palette.title)

                Text("PRECISION CALCULATOR")
                    .font(CalculatorTypography.bold(9, relativeTo: .caption2))
                    .tracking(1.2)
                    .foregroundStyle(palette.subtle)
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
            } label: {
                Image(systemName: "circle.lefthalf.filled")
                    .imageScale(.medium)
                    .frame(width: 42, height: 38)
            }
            .buttonStyle(CalculatorToolbarButtonStyle(palette: palette))
            .accessibilityLabel("Theme")

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
                        : palette.subtle
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
            .foregroundStyle(palette.muted)
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
                .foregroundStyle(palette.title)
                .lineLimit(1)
                .minimumScaleFactor(0.42)
                .frame(maxWidth: .infinity, alignment: .trailing)
                .padding(.top, 4)

            Text(model.status)
                .font(CalculatorTypography.bold(9, relativeTo: .caption2))
                .tracking(0.9)
                .foregroundStyle(
                    model.fault
                        ? palette.fault
                        : palette.subtle
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
                        .stroke(palette.border, lineWidth: 1)
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
            .foregroundStyle(palette.subtle)
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
        case .clear: return palette.warning
        case .equals: return palette.primaryText
        case .utility: return selected ? palette.title : palette.muted
        case .number, .operation: return palette.text
        }
    }

    private var border: Color {
        if selected { return palette.neutralAccent }
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
                            .stroke(palette.border, lineWidth: 1)
                    )
            )
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
                        .foregroundStyle(palette.subtle)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    ScrollView {
                        LazyVStack(spacing: sharedDesign.controlSpacing) {
                            ForEach(entries) { entry in
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

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

private struct InfiltratorPalette {
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
    let warning: Color
    let fault: Color
    let operation: Color
    let neutralAccent: Color

    static let night = InfiltratorPalette(
        background: Color(hex: 0x050608),
        panel: Color(hex: 0x101318),
        card: Color(hex: 0x171B20),
        surface: Color(hex: 0x0D1014),
        input: Color(hex: 0x0E1115),
        border: Color(hex: 0x353A40),
        text: Color(hex: 0xE8ECEF),
        title: Color(hex: 0xEEF1F3),
        muted: Color(hex: 0xAEB6BD),
        subtle: Color(hex: 0x899198),
        primary: Color(hex: 0xD7DDE2),
        primaryText: Color(hex: 0x111418),
        selected: Color(hex: 0x2B3137),
        warning: Color(hex: 0xD19E47),
        fault: Color(hex: 0xC96B6B),
        operation: Color(hex: 0x20252B),
        neutralAccent: Color(hex: 0xBEC7CF)
    )

    static let day = InfiltratorPalette(
        background: Color(hex: 0xF4F5F7),
        panel: Color(hex: 0xFFFFFF),
        card: Color(hex: 0xF8F9FA),
        surface: Color(hex: 0xECEFF2),
        input: Color(hex: 0xFFFFFF),
        border: Color(hex: 0xC7CDD3),
        text: Color(hex: 0x20252B),
        title: Color(hex: 0x111418),
        muted: Color(hex: 0x59636C),
        subtle: Color(hex: 0x737D86),
        primary: Color(hex: 0x20252B),
        primaryText: Color(hex: 0xFFFFFF),
        selected: Color(hex: 0xDDE2E7),
        warning: Color(hex: 0x9A6500),
        fault: Color(hex: 0xB54848),
        operation: Color(hex: 0xE8ECEF),
        neutralAccent: Color(hex: 0x6F7881)
    )
}

struct ContentView: View {
    @StateObject private var model = CalculatorModel()
    @Environment(\.colorScheme) private var systemColorScheme
    @AppStorage("themePreference") private var themePreferenceRaw = ThemePreference.system.rawValue

    private var themePreference: ThemePreference {
        get { ThemePreference(rawValue: themePreferenceRaw) ?? .system }
        nonmutating set { themePreferenceRaw = newValue.rawValue }
    }

    private var palette: InfiltratorPalette {
        switch themePreference {
        case .day: return .day
        case .night: return .night
        case .system:
            return systemColorScheme == .dark ? .night : .day
        }
    }

    private let columns = Array(
        repeating: GridItem(.flexible(), spacing: 10),
        count: 4
    )

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
            .padding(20)
        }
        .preferredColorScheme(themePreference.preferredScheme)
        .sheet(isPresented: $model.showingHistory) {
            HistoryView(model: model)
        }
    }

    private var header: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 1) {
                Text("Infiltrator Calc")
                    .font(.system(size: 29, weight: .regular))
                    .foregroundStyle(palette.title)

                Text("PRECISION CALCULATOR")
                    .font(.system(size: 9, weight: .bold))
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
                    .font(.system(size: 17, weight: .semibold))
                    .frame(width: 42, height: 38)
            }
            .buttonStyle(InfiltratorToolbarButtonStyle(palette: palette))
            .accessibilityLabel("Theme")

            Button {
                model.showingHistory = true
            } label: {
                Image(systemName: "clock.arrow.circlepath")
                    .font(.system(size: 17, weight: .semibold))
                    .frame(width: 42, height: 38)
            }
            .buttonStyle(InfiltratorToolbarButtonStyle(palette: palette))
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
                .font(.system(size: 12, weight: .bold))
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
            RoundedRectangle(cornerRadius: 12)
                .fill(palette.surface)
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(palette.border, lineWidth: 1)
                )
        )
    }

    private var displayCard: some View {
        VStack(spacing: 8) {
            TextField(
                "Enter an expression or variable assignment",
                text: $model.expression
            )
            .textInputAutocapitalization(.never)
            .autocorrectionDisabled()
            .multilineTextAlignment(.trailing)
            .font(.system(size: 15))
            .foregroundStyle(palette.muted)
            .padding(.horizontal, 12)
            .frame(minHeight: 42)
            .background(
                RoundedRectangle(cornerRadius: 10)
                    .fill(palette.input)
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(palette.border, lineWidth: 1)
                    )
            )
            .onSubmit { model.calculate() }

            Text(model.display)
                .font(.system(size: 46, weight: .regular, design: .rounded))
                .foregroundStyle(palette.title)
                .lineLimit(1)
                .minimumScaleFactor(0.42)
                .frame(maxWidth: .infinity, alignment: .trailing)
                .padding(.top, 4)

            Text(model.status)
                .font(.system(size: 9, weight: .bold))
                .tracking(0.9)
                .foregroundStyle(
                    model.display == "Error"
                        ? palette.fault
                        : palette.subtle
                )
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .frame(maxWidth: .infinity, alignment: .trailing)
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(palette.panel)
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(palette.border, lineWidth: 1)
                )
        )
    }

    private var keypad: some View {
        ScrollView(showsIndicators: false) {
            LazyVGrid(columns: columns, spacing: 10) {
                ForEach(Array(model.rows.enumerated()), id: \.offset) { _, row in
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
            .padding(.vertical, 1)
        }
    }

    private var footer: some View {
        Text("Keyboard ready · Variables, memory and history retained")
            .font(.system(size: 9))
            .foregroundStyle(palette.subtle)
            .frame(maxWidth: .infinity, alignment: .leading)
    }
}

private struct CalculatorKey: View {
    let title: String
    let selected: Bool
    let enabled: Bool
    let palette: InfiltratorPalette
    let action: () -> Void

    private var kind: KeyKind {
        if title == "=" { return .equals }
        if ["C", "AC", "⌫"].contains(title) { return .clear }
        if [
            "MC", "MR", "M+", "M−", "DEG", "RAD",
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
                .font(.system(size: kind == .equals ? 19 : 16, weight: .semibold))
                .frame(maxWidth: .infinity, minHeight: 54)
                .contentShape(Rectangle())
        }
        .foregroundStyle(foreground)
        .background(
            RoundedRectangle(cornerRadius: 10)
                .fill(background)
                .overlay(
                    RoundedRectangle(cornerRadius: 10)
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

private struct InfiltratorToolbarButtonStyle: ButtonStyle {
    let palette: InfiltratorPalette

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .foregroundStyle(palette.muted)
            .background(
                RoundedRectangle(cornerRadius: 10)
                    .fill(
                        configuration.isPressed
                            ? palette.card
                            : palette.surface
                    )
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
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
    @State private var history = ""

    private var themePreference: ThemePreference {
        ThemePreference(rawValue: themePreferenceRaw) ?? .system
    }

    private var palette: InfiltratorPalette {
        switch themePreference {
        case .day: return .day
        case .night: return .night
        case .system: return systemColorScheme == .dark ? .night : .day
        }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                palette.background.ignoresSafeArea()

                ScrollView {
                    Text(history)
                        .font(.system(size: 15, design: .monospaced))
                        .foregroundStyle(palette.text)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(18)
                }
                .background(palette.panel)
            }
            .navigationTitle("Calculation History")
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Clear") {
                        model.clearHistory()
                        history = model.historyText()
                    }
                    .foregroundStyle(palette.warning)
                }

                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .preferredColorScheme(themePreference.preferredScheme)
        .onAppear { history = model.historyText() }
    }
}

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

private enum InfiltratorPalette {
    static let background = Color(hex: 0x050608)
    static let panel = Color(hex: 0x101318)
    static let card = Color(hex: 0x171B20)
    static let surface = Color(hex: 0x0D1014)
    static let input = Color(hex: 0x0E1115)
    static let border = Color(hex: 0x353A40)
    static let text = Color(hex: 0xE8ECEF)
    static let title = Color(hex: 0xEEF1F3)
    static let muted = Color(hex: 0xAEB6BD)
    static let subtle = Color(hex: 0x899198)
    static let primary = Color(hex: 0xD7DDE2)
    static let primaryText = Color(hex: 0x111418)
    static let selected = Color(hex: 0x2B3137)
    static let warning = Color(hex: 0xD19E47)
    static let fault = Color(hex: 0xC96B6B)
}

struct ContentView: View {
    @StateObject private var model = CalculatorModel()

    private let columns = Array(
        repeating: GridItem(.flexible(), spacing: 10),
        count: 4
    )

    var body: some View {
        ZStack {
            InfiltratorPalette.background.ignoresSafeArea()

            VStack(spacing: 14) {
                header
                modeStrip
                displayCard
                keypad
                footer
            }
            .padding(20)
        }
        .preferredColorScheme(.dark)
        .sheet(isPresented: $model.showingHistory) {
            HistoryView(model: model)
        }
    }

    private var header: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 1) {
                Text("Infiltrator Calc")
                    .font(.system(size: 29, weight: .regular))
                    .foregroundStyle(InfiltratorPalette.title)

                Text("PRECISION DESKTOP CALCULATOR")
                    .font(.system(size: 9, weight: .bold))
                    .tracking(1.2)
                    .foregroundStyle(InfiltratorPalette.subtle)
            }

            Spacer()

            Button {
                model.showingHistory = true
            } label: {
                Image(systemName: "clock.arrow.circlepath")
                    .font(.system(size: 17, weight: .semibold))
                    .frame(width: 42, height: 38)
            }
            .buttonStyle(InfiltratorToolbarButtonStyle())
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
                        ? InfiltratorPalette.primaryText
                        : InfiltratorPalette.subtle
                )
                .frame(maxWidth: .infinity, minHeight: 38)
                .background(
                    RoundedRectangle(cornerRadius: 8)
                        .fill(
                            model.mode == mode
                                ? InfiltratorPalette.primary
                                : Color.clear
                        )
                )
            }
        }
        .padding(5)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(InfiltratorPalette.surface)
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(InfiltratorPalette.border, lineWidth: 1)
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
            .foregroundStyle(InfiltratorPalette.muted)
            .padding(.horizontal, 12)
            .frame(minHeight: 42)
            .background(
                RoundedRectangle(cornerRadius: 10)
                    .fill(InfiltratorPalette.input)
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(InfiltratorPalette.border, lineWidth: 1)
                    )
            )
            .onSubmit { model.calculate() }

            Text(model.display)
                .font(.system(size: 46, weight: .regular, design: .rounded))
                .foregroundStyle(InfiltratorPalette.title)
                .lineLimit(1)
                .minimumScaleFactor(0.42)
                .frame(maxWidth: .infinity, alignment: .trailing)
                .padding(.top, 4)

            Text(model.status)
                .font(.system(size: 9, weight: .bold))
                .tracking(0.9)
                .foregroundStyle(
                    model.display == "Error"
                        ? InfiltratorPalette.fault
                        : InfiltratorPalette.subtle
                )
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .frame(maxWidth: .infinity, alignment: .trailing)
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(InfiltratorPalette.panel)
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(InfiltratorPalette.border, lineWidth: 1)
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
                            enabled: model.keyIsEnabled(key)
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
            .foregroundStyle(InfiltratorPalette.subtle)
            .frame(maxWidth: .infinity, alignment: .leading)
    }
}

private struct CalculatorKey: View {
    let title: String
    let selected: Bool
    let enabled: Bool
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
        if selected { return InfiltratorPalette.selected }
        switch kind {
        case .number: return InfiltratorPalette.card
        case .operation: return Color(hex: 0x20252B)
        case .utility: return InfiltratorPalette.surface
        case .clear: return InfiltratorPalette.card
        case .equals: return InfiltratorPalette.primary
        }
    }

    private var foreground: Color {
        switch kind {
        case .clear: return InfiltratorPalette.warning
        case .equals: return InfiltratorPalette.primaryText
        case .utility: return selected ? InfiltratorPalette.title : InfiltratorPalette.muted
        case .number, .operation: return InfiltratorPalette.text
        }
    }

    private var border: Color {
        if selected { return Color(hex: 0xBEC7CF) }
        if kind == .equals { return InfiltratorPalette.primary }
        return InfiltratorPalette.border
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
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .foregroundStyle(InfiltratorPalette.muted)
            .background(
                RoundedRectangle(cornerRadius: 10)
                    .fill(
                        configuration.isPressed
                            ? InfiltratorPalette.card
                            : InfiltratorPalette.surface
                    )
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(InfiltratorPalette.border, lineWidth: 1)
                    )
            )
    }
}

private struct HistoryView: View {
    @ObservedObject var model: CalculatorModel
    @Environment(\.dismiss) private var dismiss
    @State private var history = ""

    var body: some View {
        NavigationStack {
            ZStack {
                InfiltratorPalette.background.ignoresSafeArea()

                ScrollView {
                    Text(history)
                        .font(.system(size: 15, design: .monospaced))
                        .foregroundStyle(InfiltratorPalette.text)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(18)
                }
                .background(InfiltratorPalette.panel)
            }
            .navigationTitle("Calculation History")
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Clear") {
                        model.clearHistory()
                        history = model.historyText()
                    }
                    .foregroundStyle(InfiltratorPalette.warning)
                }

                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .preferredColorScheme(.dark)
        .onAppear { history = model.historyText() }
    }
}

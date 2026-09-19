// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import SwiftUI

enum CalcMode: String, CaseIterable, Identifiable {
    case standard = "Standard"
    case scientific = "Scientific"
    case programmer = "Programmer"

    var id: String { rawValue }

    var bridgeValue: Int {
        switch self {
        case .standard: return 0
        case .scientific: return 1
        case .programmer: return 2
        }
    }

    init?(bridgeValue: Int) {
        switch bridgeValue {
        case 0: self = .standard
        case 1: self = .scientific
        case 2: self = .programmer
        default: return nil
        }
    }
}

@MainActor
final class CalculatorModel: ObservableObject {
    @Published var mode: CalcMode = .standard
    @Published var expression = ""
    @Published var display = "0"
    @Published var status = "READY"
    @Published var fault = false
    @Published var degrees = true
    @Published var programmerBase = 10
    @Published var programmerWidth = 64
    @Published var programmerSigned = false
    @Published var showingHistory = false

    private let bridge = CalculatorBridge()

    let standardKeys = [
        ["MC", "MR", "M+", "M−"],
        ["%", "C", "⌫", "÷"],
        ["1/x", "x²", "√", "^"],
        ["7", "8", "9", "×"],
        ["4", "5", "6", "−"],
        ["1", "2", "3", "+"],
        ["±", "0", ".", "="]
    ]

    let scientificKeys = [
        ["DEG", "π", "e", "C"],
        ["sin", "cos", "tan", "⌫"],
        ["asin", "acos", "atan", "^"],
        ["ln", "log", "exp", "x!"],
        ["√", "∛", "abs", "%"],
        ["7", "8", "9", "÷"],
        ["4", "5", "6", "×"],
        ["1", "2", "3", "−"],
        ["(", "0", ")", "+"],
        ["±", ".", "1/x", "="]
    ]

    let programmerKeys = [
        ["BIN", "OCT", "DEC", "HEX"],
        ["W8", "W16", "W32", "W64"],
        ["U/S", "~", "&", "|"],
        ["^", "<<", ">>", "AC"],
        ["(", ")", "÷", "×"],
        ["7", "8", "9", "−"],
        ["4", "5", "6", "+"],
        ["1", "2", "3", "="],
        ["0", "A", "B", "⌫"],
        ["C", "D", "E", "F"]
    ]

    var rows: [[String]] {
        switch mode {
        case .standard: return standardKeys
        case .scientific: return scientificKeys
        case .programmer: return programmerKeys
        }
    }

    func selectMode(_ newMode: CalcMode) {
        bridge.selectMode(newMode.bridgeValue)
        sync()
    }

    func setExpression(_ value: String) {
        bridge.setExpression(value)
        sync()
    }

    func press(_ key: String) {
        bridge.pressKey(key)
        sync()
    }

    func calculate() {
        bridge.pressKey("=")
        sync()
    }

    func clear() {
        bridge.pressKey(mode == .programmer ? "AC" : "C")
        sync()
    }

    func historyText() -> String {
        bridge.historyText()
    }

    func clearHistory() {
        bridge.clearHistory()
    }

    func visibleTitle(_ key: String) -> String {
        if key == "DEG" { return degrees ? "DEG" : "RAD" }
        return key
    }

    func keyIsSelected(_ key: String) -> Bool {
        switch key {
        case "DEG": return mode == .scientific && degrees
        case "RAD": return mode == .scientific && !degrees
        case "BIN": return mode == .programmer && programmerBase == 2
        case "OCT": return mode == .programmer && programmerBase == 8
        case "DEC": return mode == .programmer && programmerBase == 10
        case "HEX": return mode == .programmer && programmerBase == 16
        case "W8": return mode == .programmer && programmerWidth == 8
        case "W16": return mode == .programmer && programmerWidth == 16
        case "W32": return mode == .programmer && programmerWidth == 32
        case "W64": return mode == .programmer && programmerWidth == 64
        case "U/S": return mode == .programmer && programmerSigned
        default: return false
        }
    }

    func keyIsEnabled(_ key: String) -> Bool {
        bridge.isKeyEnabled(key)
    }

    private func sync() {
        let state = bridge.snapshot()

        if let rawMode = (state["mode"] as? NSNumber)?.intValue,
           let resolved = CalcMode(bridgeValue: rawMode) {
            mode = resolved
        }
        expression = state["expression"] as? String ?? expression
        display = state["display"] as? String ?? display
        status = state["status"] as? String ?? status
        fault = (state["fault"] as? NSNumber)?.boolValue ?? false
        degrees = (state["degrees"] as? NSNumber)?.boolValue ?? true
        programmerBase = (state["programmerBase"] as? NSNumber)?.intValue ?? 10
        programmerWidth = (state["programmerWidth"] as? NSNumber)?.intValue ?? 64
        programmerSigned =
            (state["programmerSigned"] as? NSNumber)?.boolValue ?? false
    }
}

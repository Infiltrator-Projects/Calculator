// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import SwiftUI

enum CalcMode: String, CaseIterable, Identifiable {
    case standard = "Standard"
    case scientific = "Scientific"
    case programmer = "Programmer"

    var id: String { rawValue }
}

@MainActor
final class CalculatorModel: ObservableObject {
    @Published var mode: CalcMode = .standard
    @Published var expression = ""
    @Published var display = "0"
    @Published var status = "READY"
    @Published var degrees = true
    @Published var programmerBase = 10
    @Published var programmerWidth = 64
    @Published var programmerSigned = false
    @Published var showingHistory = false

    private let bridge = ICCalculatorBridge()

    let standardKeys = [
        ["MC", "MR", "M+", "M−"],
        ["C", "⌫", "%", "÷"],
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
        case .standard: standardKeys
        case .scientific: scientificKeys
        case .programmer: programmerKeys
        }
    }

    var modeStatus: String {
        switch mode {
        case .standard:
            return "READY"
        case .scientific:
            return degrees ? "SCIENTIFIC · DEGREES" : "SCIENTIFIC · RADIANS"
        case .programmer:
            let baseName = [2: "BIN", 8: "OCT", 10: "DEC", 16: "HEX"][programmerBase] ?? "DEC"
            return "PROGRAMMER · \(baseName) · \(programmerWidth) BIT · \(programmerSigned ? "SIGNED" : "UNSIGNED")"
        }
    }

    func selectMode(_ newMode: CalcMode) {
        mode = newMode
        status = modeStatus
    }

    func press(_ key: String) {
        switch mode {
        case .standard:
            pressStandard(key)
        case .scientific:
            pressScientific(key)
        case .programmer:
            pressProgrammer(key)
        }
    }

    func calculate() {
        switch mode {
        case .programmer:
            consumeProgrammer(
                bridge.evaluateProgrammer(
                    expression,
                    base: programmerBase,
                    width: programmerWidth,
                    signedDisplay: programmerSigned
                )
            )
        case .standard, .scientific:
            consume(bridge.evaluate(expression))
        }
    }

    func clear() {
        expression = ""
        display = "0"
        status = modeStatus
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
        guard mode == .programmer else { return true }

        if ["A", "B", "C", "D", "E", "F"].contains(key) {
            return programmerBase == 16
        }
        if ["8", "9"].contains(key) {
            return programmerBase == 10 || programmerBase == 16
        }
        if ["2", "3", "4", "5", "6", "7"].contains(key) {
            return programmerBase != 2
        }
        return true
    }

    private func pressStandard(_ key: String) {
        switch key {
        case "=":
            calculate()
        case "C":
            clear()
        case "⌫":
            backspace()
        case "MC":
            bridge.memoryClear()
            status = "MEMORY CLEARED"
        case "MR":
            insert(format(bridge.memoryRecall()))
            status = "MEMORY RECALL"
        case "M+", "M−":
            let result = bridge.evaluate(expression)
            guard resultOK(result) else {
                consume(result)
                return
            }
            let value = resultValue(result)
            if key == "M+" { bridge.memoryAdd(value) }
            else { bridge.memorySubtract(value) }
            status = "MEMORY UPDATED"
        case "±", "x²", "√", "1/x":
            let result = bridge.applyUnary(key, expression: expression)
            consume(result, replaceExpression: true)
        default:
            insert(mapped(key))
        }
    }

    private func pressScientific(_ key: String) {
        switch key {
        case "DEG":
            degrees.toggle()
            status = modeStatus
        case "C":
            clear()
        case "⌫":
            backspace()
        case "=":
            calculate()
        case "π":
            insert("pi")
        case "e":
            insert("e")
        case "x!":
            insert("!")
        case "∛":
            insert("cbrt(")
        case "sin", "cos", "tan", "asin", "acos", "atan", "ln", "log", "exp", "abs":
            let result = bridge.applyScientific(key, expression: expression, degrees: degrees)
            consume(result, replaceExpression: true)
        case "±", "√", "1/x":
            let result = bridge.applyUnary(key, expression: expression)
            consume(result, replaceExpression: true)
        default:
            insert(mapped(key))
        }
    }

    private func pressProgrammer(_ key: String) {
        switch key {
        case "BIN":
            programmerBase = 2
            programmerSelectionChanged()
        case "OCT":
            programmerBase = 8
            programmerSelectionChanged()
        case "DEC":
            programmerBase = 10
            programmerSelectionChanged()
        case "HEX":
            programmerBase = 16
            programmerSelectionChanged()
        case "W8":
            programmerWidth = 8
            programmerSelectionChanged()
        case "W16":
            programmerWidth = 16
            programmerSelectionChanged()
        case "W32":
            programmerWidth = 32
            programmerSelectionChanged()
        case "W64":
            programmerWidth = 64
            programmerSelectionChanged()
        case "U/S":
            programmerSigned.toggle()
            programmerSelectionChanged()
        case "AC":
            clear()
        case "⌫":
            backspace()
        case "=":
            calculate()
        default:
            guard keyIsEnabled(key) else { return }
            insert(mapped(key))
        }
    }

    private func programmerSelectionChanged() {
        status = modeStatus
        guard !expression.isEmpty else { return }
        consumeProgrammer(
            bridge.evaluateProgrammer(
                expression,
                base: programmerBase,
                width: programmerWidth,
                signedDisplay: programmerSigned
            ),
            preserveStatus: true
        )
    }

    private func consume(_ result: NSDictionary, replaceExpression: Bool = false) {
        if resultOK(result) {
            display = result["display"] as? String ?? format(resultValue(result))
            if replaceExpression { expression = display }
            status = modeStatus
        } else {
            let error = result["error"] as? String ?? "calculation error"
            display = "Error"
            status = error.uppercased()
        }
    }

    private func consumeProgrammer(_ result: NSDictionary, preserveStatus: Bool = false) {
        if resultOK(result) {
            display = result["display"] as? String ?? "0"
            status = preserveStatus ? modeStatus : modeStatus
        } else {
            let error = result["error"] as? String ?? "programmer error"
            display = "Error"
            status = error.uppercased()
        }
    }

    private func resultOK(_ result: NSDictionary) -> Bool {
        (result["ok"] as? NSNumber)?.boolValue ?? false
    }

    private func resultValue(_ result: NSDictionary) -> Double {
        (result["value"] as? NSNumber)?.doubleValue ?? 0.0
    }

    private func mapped(_ key: String) -> String {
        switch key {
        case "×": return "*"
        case "÷": return "/"
        case "−": return "-"
        default: return key
        }
    }

    private func insert(_ text: String) {
        expression += text
        status = modeStatus
    }

    private func backspace() {
        if !expression.isEmpty { expression.removeLast() }
        status = modeStatus
    }

    private func format(_ value: Double) -> String {
        String(format: "%.15g", value)
    }
}

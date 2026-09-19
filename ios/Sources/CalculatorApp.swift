// SPDX-License-Identifier: GPL-3.0-or-later
import SwiftUI

@main
struct CalculatorApp: App {
    init() {
        CalculatorTypography.verifyBundledFonts()
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environment(\.font, CalculatorTypography.body)
        }
    }
}

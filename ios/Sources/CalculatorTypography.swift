// SPDX-License-Identifier: GPL-3.0-or-later
import CoreText
import SwiftUI
import UIKit

enum CalculatorTypography {
    private struct BundledFont {
        let resource: String
        let expectedPostScriptName: String
    }

    private static let bundledFonts = [
        BundledFont(
            resource: "mb_corpo_s_regular",
            expectedPostScriptName: "MBCorpoSTitleWEB-Regular"),
        BundledFont(
            resource: "mb_corpo_s_bold",
            expectedPostScriptName: "MBCorpoSTitleWEB-Bold"),
        BundledFont(
            resource: "mb_corpo_a_cond_regular",
            expectedPostScriptName: "MBCorpoATitleCondWEB-Regular")
    ]

    private static let resolvedPostScriptNames: [String: String] = {
        var result: [String: String] = [:]

        for font in bundledFonts {
            guard let url =
                Bundle.main.url(forResource: font.resource, withExtension: "ttf")
            else {
                preconditionFailure(
                    "Calculator bundled font is missing: \(font.resource).ttf")
            }

            _ = CTFontManagerRegisterFontsForURL(url as CFURL, .process, nil)

            guard let descriptors =
                    CTFontManagerCreateFontDescriptorsFromURL(url as CFURL)
                        as? [CTFontDescriptor],
                  let descriptor = descriptors.first
            else {
                preconditionFailure(
                    "Calculator cannot read bundled font: \(font.resource).ttf")
            }

            let ctFont = CTFontCreateWithFontDescriptor(descriptor, 12, nil)
            let postScriptName = CTFontCopyPostScriptName(ctFont) as String
            guard postScriptName == font.expectedPostScriptName else {
                preconditionFailure(
                    "Calculator font identity mismatch: \(font.resource).ttf")
            }
            result[font.resource] = postScriptName
        }

        return result
    }()

    static var uiRegularName: String {
        requiredName("mb_corpo_s_regular")
    }

    static var uiBoldName: String {
        requiredName("mb_corpo_s_bold")
    }

    static var displayName: String {
        requiredName("mb_corpo_a_cond_regular")
    }

    private static func requiredName(_ resource: String) -> String {
        guard let name = resolvedPostScriptNames[resource] else {
            preconditionFailure(
                "Calculator required font was not registered: \(resource)")
        }
        return name
    }

    private static func required(
        _ name: String,
        size: CGFloat,
        relativeTo style: Font.TextStyle
    ) -> Font {
        guard UIFont(name: name, size: size) != nil else {
            preconditionFailure(
                "Calculator refuses a non-MB text-font substitution: \(name)")
        }
        return .custom(name, size: size, relativeTo: style)
    }

    static func regular(
        _ size: CGFloat,
        relativeTo style: Font.TextStyle
    ) -> Font {
        required(uiRegularName, size: size, relativeTo: style)
    }

    static func bold(
        _ size: CGFloat,
        relativeTo style: Font.TextStyle
    ) -> Font {
        required(uiBoldName, size: size, relativeTo: style)
    }

    static func display(
        _ size: CGFloat,
        relativeTo style: Font.TextStyle
    ) -> Font {
        required(displayName, size: size, relativeTo: style)
    }

    static let body = regular(17, relativeTo: .body)

    static func verifyBundledFonts() {
        for (resource, name) in [
            ("mb_corpo_s_regular", uiRegularName),
            ("mb_corpo_s_bold", uiBoldName),
            ("mb_corpo_a_cond_regular", displayName)
        ] {
            guard UIFont(name: name, size: 12) != nil else {
                preconditionFailure(
                    "Calculator required font unavailable: \(resource).ttf")
            }
        }
    }
}

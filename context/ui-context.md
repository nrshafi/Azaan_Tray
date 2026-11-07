# UI Context

## Theme

Native Windows 11 look ("Fluent"), so the flyout feels like a built-in
system flyout (e.g. the Calendar or Quick Settings panel), not a web page.
Light and dark both supported, following the Windows app theme
(`prefers-color-scheme` inside WebView2; the host passes the current theme
and accent colour in `app.hello` for the first frame). Calm, quiet, legible:
content is mostly numbers and labels, so typography carries the design.
Decoration is minimal: no gradients, no illustration, no mosque clip-art.

The host passes the Windows accent colour. Components use it for the active
prayer, primary buttons and focus rings.

On Windows 10, or Windows 11 earlier than 22621, there is no system backdrop:
surfaces fall back to solid `--bg-base`.

## Colors

Values approximate WinUI 3 default theme resources. Verify against the
official Fluent 2 / WinUI theme resources before finalising (see
code-standards → Documentation and API Verification).

| Role | CSS Variable | Light | Dark |
| --- | --- | --- | --- |
| Page background (behind Mica/Acrylic) | `--bg-base` | `#F3F3F3` | `#202020` |
| Card / layer | `--bg-surface` | `#FFFFFF` | `#2B2B2B` |
| Subtle fill (hover rows) | `--bg-subtle` | `#0000000A` | `#FFFFFF0F` |
| Primary text | `--text-primary` | `#1A1A1A` | `#FFFFFF` |
| Muted text | `--text-muted` | `#5D5D5D` | `#C5C5C5` |
| Disabled text | `--text-disabled` | `#A0A0A0` | `#787878` |
| Primary accent (from system) | `--accent-primary` | system, fallback `#005FB8` | system, fallback `#60CDFF` |
| Text on accent | `--accent-on` | `#FFFFFF` | `#000000` |
| Border | `--border-default` | `#E5E5E5` | `#3A3A3A` |
| Forbidden window / caution | `--state-caution` | `#9D5D00` | `#FCE100` |
| Error | `--state-error` | `#C42B1C` | `#FF99A4` |
| Success | `--state-success` | `#0F7B0F` | `#6CCB5F` |

### Taskbar widget colours (native, Direct2D)

| State | On dark taskbar | On light taskbar |
| --- | --- | --- |
| Normal text | `#FFFFFF` | `#000000` |
| Secondary line | `#FFFFFF` @ 78 % | `#000000` @ 62 % |
| Soon (≤ threshold) | accent colour (light variant) | accent colour (dark variant) |
| Forbidden window | `#FCE100` | `#9D5D00` |

The taskbar theme comes from `HKCU\...\Themes\Personalize\SystemUsesLightTheme`,
re-read on `WM_SETTINGCHANGE` ("ImmersiveColorSet").

## Typography

| Role | Font | Variable |
| --- | --- | --- |
| UI text | `"Segoe UI Variable Text", "Segoe UI", system-ui, sans-serif` | `--font-sans` |
| Display (large countdown) | `"Segoe UI Variable Display", "Segoe UI", sans-serif` | `--font-display` |
| Numbers | same as UI with `font-variant-numeric: tabular-nums` | — |
| Arabic/Hijri labels | `"Segoe UI", "Arial", sans-serif` (system Arabic shaping) | `--font-arabic` |

Type ramp (Fluent): Caption 12/16, Body 14/20, Body Strong 14/20 semibold,
Subtitle 20/28 semibold, Title 28/36 semibold, Display 40/52 semibold (the
countdown only). The taskbar widget uses Segoe UI Variable Small/Text at
9 pt × DPI scale, both lines tabular-nums.

## Border Radius

| Context | Class |
| --- | --- |
| Inline / small UI (buttons, inputs, chips) | `rounded-[4px]` |
| Cards / panels / list groups | `rounded-[8px]` |
| Flyout / dialogs (DWM rounds the window itself) | `rounded-[8px]` |

## Component Library

shadcn/ui (Radix primitives + Tailwind), restyled to the tokens above.
Components live in `ui/src/components/ui/` and are generated with the shadcn
CLI (pinned version, see `ui/package.json`), not hand-written. Expected set:
Button, Switch, Select, Slider, Tabs, Input, Label, Tooltip, Separator,
ScrollArea, Command (city search), Table. Do not add components the views
don't use.

App-specific components live in `ui/src/components/`: `PrayerRow`,
`CountdownHero`, `ForbiddenBadge`, `MonthTable`, `SettingsSection`,
`AudioPicker`.

## Layout Patterns

- **Today flyout**: 360 × ~520 logical px, anchored above the taskbar widget
  (or tray icon), right-aligned to it, 12 px gap. Top: Hijri + Gregorian
  date, then `CountdownHero` (next prayer name, countdown, progress bar
  through the current window). Below: a list of `PrayerRow`s (name, adhan
  time, jamaat time, bell icon state); the current prayer is highlighted
  with an accent bar on the left. Footer: location name, gear button
  (opens Settings), calendar button (opens Month).
- **Settings window**: 760 × 560, resizable, normal window with title bar
  (DWM dark mode + Mica). Left nav list (Location, Calculation,
  Adjustments, Jamaat, Alerts and Audio, Taskbar and Appearance, Startup,
  About), right content pane of `SettingsSection` cards. Changes apply
  instantly (no Save button), as in Windows Settings.
- **Month view**: opens in the Settings window frame. Month/year picker at
  top, a sticky-header table with one row per day, today highlighted, Fridays
  marked. A print stylesheet hides chrome.
- **Spacing**: 4 px grid; card padding 16, section gap 24, row height 40.
- **Motion**: flyout content fades and slides up 8 px over 150 ms (disabled
  with reduced motion). No looping animations, except the countdown text
  update, which is not animated.

## Taskbar Widget Layout (native)

- Two lines, right edge 6 px left of the notification area, vertically
  centred in the taskbar. Width = max text width + 2 × 6 px padding,
  recalculated only when the text template changes (not every second).
- Line 1: `{PrayerName}  -HH:MM:SS` (or `-H:MM` in minutes mode).
  Line 2: `Ends HH:MM` | `Jmt HH:MM` | `Iftar HH:MM`, by setting.
- Forbidden window: line 1 becomes `Forbidden  -MM:SS` in caution colour.
- Hover: native tooltip with the full day. Left-click opens the Today
  flyout. Right-click opens the tray menu.

## Icons

Lucide React, stroke icons only. Sizes: 16 px inline, 20 px in buttons.
The tray icon is a custom 16/20/24/32 px `.ico` (crescent mark), with
light- and dark-taskbar variants. When Mute is active, it shows a small
slash overlay.

## Accessibility

- WCAG 2.2 AA contrast for all text on both themes (check the caution colour
  on light especially).
- Full keyboard navigation: Esc closes the flyout, Tab order logical, visible
  focus ring (2 px accent).
- Countdown region has `aria-live="off"`. A separate visually hidden status
  announces only prayer changes, not every second.
- The native widget exposes its text via the tooltip and window name for
  Narrator.

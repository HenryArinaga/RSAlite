# RSAlite Modern UI

## Quick Start

### Build and Run
```bash
make rsalite-gui-modern
./rsalite-gui-modern
```

### Basic Usage
1. Enter a number in the Home page
2. Click the blue "Factor" button
3. View results below

### Change Method
- Click "Methods" in sidebar
- Select an algorithm from the list

### Configure Settings
- Click "Settings" in sidebar
- Toggle Sieve, SIMD, or Benchmarking
- Note: Sieve only works with Trial Division
- Note: SIMD only works with Trial Division and SQRT methods

### View History
- Click "Logs" in sidebar
- Export CSV or clear history

## What's New

### Modern GNOME Design
- **GtkHeaderBar** - Clean titlebar with integrated controls
- **Sidebar Navigation** - Easy access to different sections
- **Multi-page Layout** - Organized into Home, Methods, Settings, and Logs
- **Modern Widgets** - Switches instead of toggle buttons, list-based selection
- **Generous Spacing** - 24px margins following GNOME HIG
- **GTK Style Classes** - Proper use of suggested-action, destructive-action

### Interface Structure
```
┌─────────────────────────────────────────┐
│  RSAlite              [?] [≡]           │ ← Header Bar
├──────────┬────────────────────────────┤
│ Home     │  Prime Factorization       │
│ Methods  │  [Input and controls]      │
│ Settings │  [Results display]         │
│ Logs     │                            │
└──────────┴────────────────────────────┘
```

## Files

- `ui/interface_modern.glade` - Modern GTK interface definition
- `gui_modern.c` - Modern GUI implementation
- Original files (`ui/interface.glade`, `gui_main.c`) remain unchanged

## Key Improvements

### Better Organization
- **Home**: Main factorization interface
- **Methods**: Algorithm selection with descriptions
- **Settings**: Optimization toggles with explanations
  - Sieve optimization (Trial Division only)
  - SIMD acceleration (Trial Division and SQRT only)
  - Benchmarking mode (all methods)
- **Logs**: Operation history with export

### Modern Controls
- GtkSwitch for settings (instead of toggle buttons)
- GtkListBox for method selection (instead of buttons)
- Descriptive subtitles for all options
- Clear visual hierarchy

### Enhanced Feedback
- Spinner shows operation in progress
- Cancel button for long operations
- Disabled controls during computation
- Real-time log updates

## Design Principles

### GNOME HIG Compliance
- Large click targets (40px minimum)
- Generous whitespace (24px margins)
- Grouped controls in frames
- Consistent spacing (12px between items)
- Clear visual hierarchy

### Accessibility
- Keyboard navigation support
- Proper label associations
- Screen reader friendly
- High contrast through themes

## Comparison with Original

| Aspect | Original | Modern |
|--------|----------|--------|
| Layout | Single page grid | Multi-page with sidebar |
| Navigation | Scroll | Page-based |
| Method Selection | Buttons | List with descriptions |
| Settings | Toggle buttons | Switches with subtitles |
| Logs | Menu export only | Dedicated page |
| Spacing | Compact (12px) | Generous (24px) |

## Technical Details

### Components Used
- GtkApplicationWindow
- GtkHeaderBar
- GtkStack + GtkStackSidebar
- GtkListBox
- GtkSwitch
- GtkFrame
- GtkScrolledWindow

### Features
- Threaded factorization for responsiveness
- Cancellation support
- Property bindings for settings
- Automatic log updates
- CSV export functionality

## Troubleshooting

### Build Issues
```bash
# Check GTK3 is installed
pkg-config --modversion gtk+-3.0

# Rebuild from scratch
make clean && make rsalite-gui-modern
```

### Runtime Issues
- Ensure `ui/interface_modern.glade` exists
- Verify GTK3 libraries are available
- Check for error messages in terminal

## Both GUIs Available

The original GUI still works:
```bash
make rsalite-gui
./rsalite-gui
```

Choose based on preference:
- **Modern**: Spacious, organized, GNOME-native
- **Original**: Compact, all-in-one, traditional

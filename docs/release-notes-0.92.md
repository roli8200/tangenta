## TANGENTA 0.92

### New
- **AI image editing** (Bitmaps → Optimize Image with AI): an image model (OpenAI gpt-image or Google Gemini,
  with your own API key) edits the photo – remove colour casts, fix exposure and contrast, natural colours,
  sky, sharpen, denoise, smooth skin, restore old photos, colourize black-and-white pictures, plus a wish in
  your own words. TANGENTA **transfers colour and light onto your original**, so resolution, colour depth and
  sharpness stay; for content changes you can take the AI image itself. Every request adds a variant,
  strength and sliders fine-tune it. The classic mode, where the AI only suggests slider values, is still there.
- **Cut out** (Bitmaps → Cut Out): removes the background locally with a segmentation model (BiRefNet,
  nothing leaves your computer) or via OpenAI
- **Cutout Assistant** for difficult edges: paint a green band over the edge, fill the inside, and TANGENTA
  computes the opacity of every pixel (closed-form matting) – fine hair stays, colour fringes are removed.
  Keep/remove detail brushes, edge shift, feather, hardness, preview on different backgrounds
- **Your own toolbars**: create toolbars, drag commands from Options → Customization → Commands into them,
  rearrange, add separators, dock or float them; every toolbar has a grip for moving it
- **Icons on every menu command**
- **Baseline grid** for paragraph text: lines snap to the grid, set top/spacing/bottom in mm, drag single lines
- **CDR export** (version 16/X6 or 13/X3): curves, fills, outlines, layers and pages
- **CDR import** now brings in transparency, PowerClip, gradients, mesh fills and shifted characters
- **Multiple documents** in their own windows (Window menu, Ctrl+F6), clipboard exchange with other programs
  (SVG, PNG, bitmaps, text), adjustable interface size
- **Splash screen** and a new **Help** menu with *About TANGENTA*
- Flatter document window buttons and an aligned document icon in the menu bar
- All new texts translated into English, French, Italian, Spanish, Portuguese and Simplified Chinese

### Packages
| File | System |
|---|---|
| `TANGENTA-0.92-x64-setup.exe` | Windows 10/11, setup wizard (`/quiet`, `/passive`) |
| `TANGENTA-0.92-x64.msi` | Windows, network deployment (`msiexec /i … /qn`) |
| `TANGENTA-0.92-macos.dmg` | macOS 13 or newer, Apple silicon and Intel |
| `tangenta_0.92-1.debian12_amd64.deb` etc. | Debian 12/13, Ubuntu 24.04/26.04 (`sudo apt install ./…`) |
| `TANGENTA-0.92-x86_64.flatpak` | every Linux system with Flatpak (`flatpak install --user …`) |

**AI features** need your own API key (OpenAI or Google Gemini for image editing); costs are billed by the provider.
**Local cut-out** uses ONNX Runtime, which is included in the Windows packages. On Linux and macOS install
ONNX Runtime yourself (or point `VEK_ONNXRUNTIME` to the library) – the Cutout Assistant and the AI image editing
work without it.

**macOS:** the Mac version is not notarized by Apple yet. On first start open *System Settings → Privacy & Security*
and click *Open Anyway*, or run `xattr -dr com.apple.quarantine /Applications/TANGENTA.app`.

**Windows:** the packages are not signed yet. If Windows shows "Windows protected your PC", click *More info* → *Run anyway*.

Checksums: `SHA256SUMS-0.92.txt` (`sha256sum --ignore-missing -c SHA256SUMS-0.92.txt`).

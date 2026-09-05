# Build without installing Xcode locally

1. Create a public GitHub repository.
2. Upload the CONTENTS of this folder, including the hidden `.github` folder.
3. Commit to `main`.
4. Open **Actions**.
5. Select **Build Physical Drum Engine**.
6. The workflow starts automatically after the commit. You can also use **Run workflow**.
7. When green, open the run and download **Physical-Drum-Engine-macOS** under Artifacts.

The artifact contains the AU `.component`, VST3 `.vst3`, and Standalone `.app` produced on a hosted macOS runner.

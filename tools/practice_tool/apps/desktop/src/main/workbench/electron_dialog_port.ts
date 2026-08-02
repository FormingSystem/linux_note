import { dialog, type BrowserWindow } from "electron";
import type { dialog_port } from "./workbench_controller.mts";

export class electron_dialog_port implements dialog_port {
  async choose_markdown(owner: BrowserWindow): Promise<string | null> {
    const result = await dialog.showOpenDialog(owner, {
      title: "打开 Markdown",
      properties: ["openFile"],
      filters: [{ name: "Markdown", extensions: ["md", "markdown"] }],
    });
    return result.canceled || result.filePaths.length !== 1 ? null : result.filePaths[0] ?? null;
  }

  async choose_folder(owner: BrowserWindow): Promise<string | null> {
    const result = await dialog.showOpenDialog(owner, {
      title: "打开文件夹",
      properties: ["openDirectory"],
    });
    return result.canceled || result.filePaths.length !== 1 ? null : result.filePaths[0] ?? null;
  }
}

import { dialog, type BrowserWindow } from "electron";
import type { dialog_port } from "./workbench_controller.mts";

export class electron_dialog_port implements dialog_port {
  private readonly markdown_fixture: string | null;
  private readonly folder_fixture: string | null;

  constructor(
    markdown_fixture: string | null = null,
    folder_fixture: string | null = null,
  ) {
    this.markdown_fixture = markdown_fixture;
    this.folder_fixture = folder_fixture;
  }

  async choose_markdown(owner: BrowserWindow): Promise<string | null> {
    if (this.markdown_fixture !== null) return this.markdown_fixture;
    const result = await dialog.showOpenDialog(owner, {
      title: "打开 Markdown",
      properties: ["openFile"],
      filters: [{ name: "Markdown", extensions: ["md", "markdown"] }],
    });
    return result.canceled || result.filePaths.length !== 1 ? null : result.filePaths[0] ?? null;
  }

  async choose_folder(owner: BrowserWindow): Promise<string | null> {
    if (this.folder_fixture !== null) return this.folder_fixture;
    const result = await dialog.showOpenDialog(owner, {
      title: "打开文件夹",
      properties: ["openDirectory"],
    });
    return result.canceled || result.filePaths.length !== 1 ? null : result.filePaths[0] ?? null;
  }
}

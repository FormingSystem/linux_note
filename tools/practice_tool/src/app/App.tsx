import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import AppShell from "./AppShell";
import LobbyPage from "../features/lobby/LobbyPage";
import LibraryPage from "../features/training-library/LibraryPage";
import UnitDetailPage from "../features/training-library/UnitDetailPage";
import WorkspaceManager from "../features/training-management/WorkspaceManager";
import SessionPage from "../features/practice-sessions";
import SimplePage from "../shared/components/SimplePage";

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route element={<AppShell />}>
          <Route index element={<LobbyPage />} />
          <Route path="library" element={<LibraryPage />} />
          <Route path="library/units/:unitId" element={<UnitDetailPage />} />
          <Route path="my-training" element={<WorkspaceManager />} />
          <Route path="sessions/:sessionId/:stage/:itemId" element={<SessionPage />} />
          <Route path="review" element={<SimplePage title="复习计划" description="完成训练后，系统会在这里汇总需要重建和部分输出的知识点。" />} />
          <Route path="history" element={<SimplePage title="训练历史" description="训练记录已经保存在本机；历史筛选和对比将在下一迭代开放。" />} />
          <Route path="sources" element={<SimplePage title="知识源" description="训练内容通过稳定文档 ID 指向权威知识源，原文可在训练题内追溯。" />} />
          <Route path="settings" element={<SimplePage title="设置" description="这里将承载显示、训练节奏、数据备份和隐私设置。" />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}

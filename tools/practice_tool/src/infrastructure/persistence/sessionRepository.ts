import type { LearningChapter, LoadedUnit, PracticeSession, TopicBook, TrainingStage } from "../../shared/types";
import { readAllRecords, readRecord, STORES, writeRecord } from "./database";

function firstItem(content: LoadedUnit) {
  return content.learning[0]?.id ?? "";
}

export function createSession(content: LoadedUnit): PracticeSession {
  const now = new Date().toISOString();
  return {
    id: crypto.randomUUID(),
    unitId: content.unit.id,
    unitTitle: content.unit.title,
    status: "in_progress",
    currentStage: "learning",
    currentItemId: firstItem(content),
    contentSnapshot: structuredClone(content),
    answers: {},
    ratings: {},
    hintLevels: {},
    revealedItemIds: [],
    completedItemIds: [],
    stagePositions: { learning: firstItem(content) },
    createdAt: now,
    updatedAt: now,
    completedAt: null,
    revision: 1,
  };
}

export function saveSession(session: PracticeSession): Promise<void> {
  return writeRecord(STORES.sessions, session);
}

export function loadSession(id: string): Promise<PracticeSession | undefined> {
  return readRecord<PracticeSession>(STORES.sessions, id).then((session) => session ? normalizeSession(session) : undefined);
}

function normalizeSession(session: PracticeSession): PracticeSession {
  const learning = session.contentSnapshot.learning.map((guide, index) => {
    const legacy = guide as LearningChapter & {
      content_file?: string;
      reading?: Array<{ heading: string; content: string }>;
    };
    if (legacy.content_markdown && legacy.file) return guide;
    const contentMarkdown = (legacy.reading ?? []).map((section) => `## ${section.heading}\n\n${section.content}`).join("\n\n");
    const { reading: _reading, content_file: _contentFile, ...current } = legacy;
    return {
      ...current,
      file: `migrated/P${String(index + 1).padStart(2, "0")}_session_snapshot.md`,
      estimated_minutes: legacy.estimated_minutes ?? 10,
      prerequisites: legacy.prerequisites ?? [],
      claim_ids: legacy.claim_ids ?? [],
      content_markdown: legacy.content_markdown || contentMarkdown,
    };
  });
  const existingBook = (session.contentSnapshot as LoadedUnit & { book?: TopicBook }).book;
  const book: TopicBook = existingBook ?? {
    schema_version: 1,
    id: `${session.unitId}.migrated-book`,
    title: session.unitTitle,
    version: "0.0.0",
    status: "archived",
    outline_file: "migrated_outline.md",
    outline_markdown: "# 已迁移的历史学习导引\n\n本会话创建于专题电子书协议启用之前，正文已经转换为只读章节快照。",
    chapters: learning,
    claims: [],
  };
  return {
    ...session,
    contentSnapshot: { ...session.contentSnapshot, book: { ...book, chapters: learning }, learning },
    stagePositions: session.stagePositions ?? { [session.currentStage]: session.currentItemId },
  };
}

export async function listSessions(): Promise<PracticeSession[]> {
  const sessions = await readAllRecords<PracticeSession>(STORES.sessions);
  return sessions.sort((left, right) => right.updatedAt.localeCompare(left.updatedAt));
}

export async function findActiveSession(unitId: string): Promise<PracticeSession | undefined> {
  const sessions = await listSessions();
  return sessions.find((session) => session.unitId === unitId && session.status === "in_progress");
}

export function stageItems(session: PracticeSession, stage: TrainingStage): Array<{ id: string }> {
  if (stage === "learning") return session.contentSnapshot.learning;
  if (stage === "guided") return session.contentSnapshot.guided;
  if (stage === "reconstruction") return session.contentSnapshot.models;
  if (stage === "professional") return session.contentSnapshot.cases;
  return [];
}

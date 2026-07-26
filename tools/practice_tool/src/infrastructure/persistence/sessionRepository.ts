import type { LoadedUnit, PracticeSession, TrainingStage } from "../../shared/types";
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
  return readRecord<PracticeSession>(STORES.sessions, id);
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

export type Rating = "again" | "hard" | "good";

export type KnowledgeRef = {
  source_id: string;
  id: string;
  path: string;
};

export type LearningChapter = {
  id: string;
  title: string;
  objective: string;
  file: string;
  content_markdown: string;
  estimated_minutes: number;
  prerequisites: string[];
  claim_ids: string[];
  check_questions: Array<{ question: string; answer: string }>;
  open_associations: string[];
  topology_memory: {
    prompt: string;
    nodes: string[];
    links: string[];
  };
  knowledge_refs: string[];
};

export type KnowledgeClaim = {
  id: string;
  statement: string;
  type: string;
  status: "candidate" | "reviewing" | "verified" | "conflicting" | "version_bound" | "superseded" | "rejected";
  authority_chapter: string;
  evidence_ids: string[];
};

export type TopicBook = {
  schema_version: 1;
  id: string;
  title: string;
  version: string;
  status: string;
  outline_file: string;
  outline_markdown: string;
  chapters: LearningChapter[];
  claims: KnowledgeClaim[];
};

export type GuidedQuestion = {
  id: string;
  chapter_ids: string[];
  claim_ids: string[];
  title: string;
  scenario: string;
  question: string;
  hints: string[];
  answer_framework: string[];
  common_mistakes: string[];
  knowledge_refs: string[];
};

export type ModelTask = {
  id: string;
  chapter_ids: string[];
  claim_ids: string[];
  title: string;
  output_type: "sequence" | "causal_model" | "state_ownership";
  prompt: string;
  constraints: string[];
  required_outputs: string[];
  verification_questions: string[];
  knowledge_refs: string[];
};

export type ProfessionalCase = {
  id: string;
  chapter_ids: string[];
  claim_ids: string[];
  title: string;
  difficulty: "intermediate" | "advanced";
  background: string;
  evidence: string[];
  questions: string[];
  rubric: {
    diagnosis: string[];
    solution: string[];
    unavoidable_costs: string[];
    boundaries: string[];
  };
  knowledge_refs: string[];
};

export type PracticeUnit = {
  schema_version: 3;
  id: string;
  title: string;
  subtitle: string;
  status: string;
  estimated_minutes: number;
  knowledge_refs: KnowledgeRef[];
  stages: {
    learning: { title: string; purpose: string; book_file: string };
    guided: { title: string; purpose: string; items_file: string };
    reconstruction: { title: string; purpose: string; items_file: string };
    professional: { title: string; purpose: string; items_file: string };
  };
};

export type UnitCatalogItem = {
  id: string;
  title: string;
  summary: string;
  domain: string;
  topic: string;
  module: string;
  level: string;
  estimated_minutes: number;
  status: "available" | "draft" | "archived";
  unit_file: string;
  tags: string[];
};

export type UnitCatalog = { schema_version: number; units: UnitCatalogItem[] };

export type LoadedUnit = {
  unit: PracticeUnit;
  book: TopicBook;
  learning: LearningChapter[];
  guided: GuidedQuestion[];
  models: ModelTask[];
  cases: ProfessionalCase[];
};

export type TrainingCategory = {
  id: string;
  name: string;
  parentId: string | null;
  description: string;
  trashed: boolean;
};

export type UserTrainingModule = {
  id: string;
  name: string;
  description: string;
  unitIds: string[];
  categoryIds: string[];
  trashed: boolean;
  createdAt: string;
  updatedAt: string;
};

export type WorkspaceData = {
  schemaVersion: 1;
  categories: TrainingCategory[];
  modules: UserTrainingModule[];
};

export type TrainingStage = "learning" | "guided" | "reconstruction" | "professional" | "summary";
export type SessionStatus = "in_progress" | "paused" | "completed" | "abandoned";

export type PracticeSession = {
  id: string;
  unitId: string;
  unitTitle: string;
  status: SessionStatus;
  currentStage: TrainingStage;
  currentItemId: string;
  contentSnapshot: LoadedUnit;
  answers: Record<string, string>;
  ratings: Record<string, Rating>;
  hintLevels: Record<string, number>;
  revealedItemIds: string[];
  completedItemIds: string[];
  stagePositions: Partial<Record<TrainingStage, string>>;
  createdAt: string;
  updatedAt: string;
  completedAt: string | null;
  revision: number;
};

export type SaveState = "clean" | "dirty" | "saving" | "saved" | "failed";

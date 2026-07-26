export type Rating = "again" | "hard" | "good";

export type KnowledgeRef = {
  source_id: string;
  id: string;
  path: string;
};

export type LearningGuide = {
  id: string;
  title: string;
  objective: string;
  reading: Array<{ heading: string; content: string }>;
  check_questions: Array<{ question: string; answer: string }>;
  open_associations: string[];
  topology_memory: {
    prompt: string;
    nodes: string[];
    links: string[];
  };
  knowledge_refs: string[];
};

export type GuidedQuestion = {
  id: string;
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
  schema_version: number;
  id: string;
  title: string;
  subtitle: string;
  status: string;
  estimated_minutes: number;
  knowledge_refs: KnowledgeRef[];
  stages: {
    learning: { title: string; purpose: string; items_file: string };
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
  learning: LearningGuide[];
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

export type Review = {
  unitId: string;
  createdAt: string;
  learningAnswers: Record<string, string>;
  guidedAnswers: Record<string, string>;
  modelAnswers: Record<string, string>;
  caseAnswers: Record<string, string>;
  ratings: Record<string, Rating>;
};

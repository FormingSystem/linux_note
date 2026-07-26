export type Rating = "again" | "hard" | "good";

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
  knowledge_refs: { id: string; path: string }[];
  stages: {
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

export type UnitCatalog = {
  schema_version: number;
  units: UnitCatalogItem[];
};

export type Review = {
  unitId: string;
  createdAt: string;
  guidedAnswers: Record<string, string>;
  modelAnswers: Record<string, string>;
  caseAnswers: Record<string, string>;
  ratings: Record<string, Rating>;
};

import type { TrainingStage } from "../../../shared/types";

export const STAGES: Array<{ id: TrainingStage; label: string }> = [
  { id: "learning", label: "专题学习" },
  { id: "guided", label: "提示提问" },
  { id: "reconstruction", label: "脱稿输出" },
  { id: "professional", label: "专业案例" },
  { id: "summary", label: "训练总结" },
];

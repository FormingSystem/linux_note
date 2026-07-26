/// <reference types="vite/client" />

declare module "virtual:practice-runtime-config" {
  const config: {
    schema_version: number;
    config_source: string | null;
    sources: Array<{
      id: string;
      title: string;
      kind: "filesystem" | "http";
      location: string;
    }>;
  };
  export default config;
}

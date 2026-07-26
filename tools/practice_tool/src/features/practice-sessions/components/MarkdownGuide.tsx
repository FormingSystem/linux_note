import Markdown from "react-markdown";
import { isValidElement, type ReactElement, type ReactNode } from "react";
import { toString } from "mdast-util-to-string";
import rehypeHighlight from "rehype-highlight";
import remarkParse from "remark-parse";
import remarkGfm from "remark-gfm";
import { unified } from "unified";
import { visit } from "unist-util-visit";
import type { Code, Heading, Root } from "mdast";
import MermaidDiagram from "./MermaidDiagram";

export type MarkdownHeading = {
  id: string;
  level: 2 | 3 | 4;
  title: string;
  line: number;
};

export function markdownPlainText(value: string) {
  return value
    .replace(/\\([\\`*{}[\]()#+.!_>-])/g, "$1")
    .replace(/[*`[\]]/g, "")
    .trim();
}

export function markdownHeadings(markdown: string): MarkdownHeading[] {
  const used = new Map<string, number>();
  const headings: MarkdownHeading[] = [];
  const visibleMarkdown = markdown.replace(/^\uFEFF?---\s*\r?\n[\s\S]*?\r?\n---\s*(?:\r?\n|$)/, "");
  const tree = unified().use(remarkParse).parse(visibleMarkdown);
  visit(tree, "heading", (node: Heading) => {
    if (node.depth < 2 || node.depth > 4) return;
    const title = markdownPlainText(toString(node));
    const base = title
      .toLocaleLowerCase()
      .replace(/[^\p{Letter}\p{Number}_-]+/gu, "-")
      .replace(/^-+|-+$/g, "") || "section";
    const count = used.get(base) ?? 0;
    used.set(base, count + 1);
    headings.push({
      id: count === 0 ? base : `${base}-${count + 1}`,
      level: node.depth as 2 | 3 | 4,
      title,
      line: node.position?.start.line ?? 0,
    });
  });
  return headings;
}

function remarkInferCodeLanguage() {
  return (tree: Root) => {
    visit(tree, "code", (node: Code) => {
      if (node.lang) return;
      node.lang = inferCodeLanguage(node.value);
    });
  };
}

function inferCodeLanguage(source: string) {
  const value = source.trim();
  if (!value) return undefined;
  if (/^[\[{][\s\S]*[\]}]$/.test(value)) {
    try {
      JSON.parse(value);
      return "json";
    } catch {
      // Braces are also common in C/C++; continue with structural checks.
    }
  }

  const lines = value.split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
  const cStatements = lines.filter((line) =>
    /^(?:#\s*(?:include|define|if|ifdef|ifndef|endif)\b|(?:static\s+)?(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:struct\s+\w+\s*\{?|void|char|short|int|long|bool|size_t|u(?:8|16|32|64)|s(?:8|16|32|64))\b|(?:return|if|else|for|while|switch|case|break|continue)\b|[A-Za-z_]\w*(?:->\w+|\.\w+|\[[^\]]+\])?\s*(?:=|\+\+|--|\+=|-=)|[A-Za-z_]\w*\s*\([^)]*\)\s*;|[{}];?)/
      .test(line),
  ).length;
  const cPunctuation = /[;{}]|->|(?:^|\s)=(?:\s|$)|==|!=|&&|\|\||\b(?:NULL|GFP_\w+|container_of|sizeof)\b/.test(value);
  if (cPunctuation && cStatements >= Math.max(1, Math.ceil(lines.length * 0.6))) return "c";

  const shellStatements = lines.filter((line) =>
    /^(?:#!\/.*\b(?:ba)?sh\b|(?:sudo\s+)?(?:cd|export|source|printf|echo|test|git|npm|node|make|cmake|apt|pacman|curl|wget)\b|[A-Za-z_]\w*=)/.test(line),
  ).length;
  if (shellStatements >= Math.max(1, Math.ceil(lines.length * 0.75))) return "bash";
  return undefined;
}

export default function MarkdownGuide({ markdown, headings = markdownHeadings(markdown) }: { markdown: string; headings?: MarkdownHeading[] }) {
  const visibleMarkdown = markdown.replace(/^\uFEFF?---\s*\r?\n[\s\S]*?\r?\n---\s*(?:\r?\n|$)/, "");
  const headingIds = new Map(headings.map((entry) => [`${entry.level}:${entry.line}`, entry.id]));
  const heading = (level: 2 | 3 | 4) => ({ children, node }: { children?: React.ReactNode; node?: { position?: { start: { line: number } } } }) => {
    const id = headingIds.get(`${level}:${node?.position?.start.line ?? 0}`);
    const Tag = `h${level}` as const;
    return <Tag id={id}>{children}</Tag>;
  };

  return (
    <article className="learning-document">
      <Markdown
        remarkPlugins={[remarkGfm, remarkInferCodeLanguage]}
        rehypePlugins={[[rehypeHighlight, { detect: false, ignoreMissing: true, plainText: ["mermaid"] }]]}
        components={{
          h2: heading(2),
          h3: heading(3),
          h4: heading(4),
          code: ({ className, children }) => className === "language-mermaid"
            ? <MermaidDiagram source={String(children).replace(/\n$/, "")} />
            : <code className={className}>{children}</code>,
          pre: ({ children }) => isMermaidElement(children) ? <>{children}</> : <pre>{children}</pre>,
        }}
      >
        {visibleMarkdown}
      </Markdown>
    </article>
  );
}

function isMermaidElement(children: ReactNode) {
  if (!isValidElement(children)) return false;
  return (children as ReactElement<{ className?: string }>).props.className === "language-mermaid"
    || (children as ReactElement).type === MermaidDiagram;
}

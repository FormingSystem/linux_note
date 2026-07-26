import Markdown from "react-markdown";
import remarkGfm from "remark-gfm";

export default function MarkdownGuide({ markdown }: { markdown: string }) {
  return (
    <article className="learning-document">
      <Markdown remarkPlugins={[remarkGfm]}>{markdown}</Markdown>
    </article>
  );
}

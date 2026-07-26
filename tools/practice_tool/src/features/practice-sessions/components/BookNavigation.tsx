import type { LearningChapter, TopicBook } from "../../../shared/types";
import Markdown from "react-markdown";

export default function BookNavigation({ book, activeChapterId, completed, onSelect }: {
  book: TopicBook;
  activeChapterId: string;
  completed: Set<string>;
  onSelect: (chapter: LearningChapter) => void;
}) {
  return (
    <aside className="book-navigation" aria-label="电子书章节目录">
      <div className="book-navigation-heading">
        <span>专题电子书</span>
        <strong>{book.title}</strong>
        <small>版本 {book.version}</small>
      </div>
      <details>
        <summary>查看目录大纲</summary>
        <div className="outline-preview"><Markdown>{book.outline_markdown}</Markdown></div>
      </details>
      <ol>
        {book.chapters.map((chapter, index) => (
          <li key={chapter.id}>
            <button className={chapter.id === activeChapterId ? "active" : ""} onClick={() => onSelect(chapter)}>
              <span>{completed.has(chapter.id) ? "✓" : String(index + 1).padStart(2, "0")}</span>
              <span><strong>{chapter.title}</strong><small>{chapter.estimated_minutes} 分钟</small></span>
            </button>
          </li>
        ))}
      </ol>
    </aside>
  );
}

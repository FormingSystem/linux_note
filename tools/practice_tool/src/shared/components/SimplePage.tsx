import { Link } from "react-router-dom";

export default function SimplePage({ title, description }: { title: string; description: string }) {
  return (
    <section className="panel simple-page">
      <div className="eyebrow">Loop Workspace</div>
      <h1>{title}</h1>
      <p>{description}</p>
      <Link className="button-link primary" to="/">返回大厅</Link>
    </section>
  );
}

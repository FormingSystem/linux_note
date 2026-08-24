const page_body = document.body;
const menu_button = document.querySelector("[data_action='toggle_sidebar']");
const close_targets = document.querySelectorAll("[data_action='close_sidebar']");
const search_input = document.querySelector("[data_search_input]");
const search_results = document.querySelector("[data_search_results]");
const progress_bar = document.querySelector("[data_progress_bar]");
const toolbar_title = document.querySelector("[data_toolbar_title]");
const chapter_elements = [...document.querySelectorAll("article[data_chapter]")];
const toc_links = [...document.querySelectorAll("[data_toc_link]")];
const toc_section_links = [...document.querySelectorAll("[data_toc_section_link]")];

function set_sidebar(open_state) {
  page_body.dataset.sidebar_open = open_state ? "true" : "false";
  menu_button?.setAttribute("aria-expanded", open_state ? "true" : "false");
}

menu_button?.addEventListener("click", () => {
  set_sidebar(page_body.dataset.sidebar_open !== "true");
});

for (const close_target of close_targets) {
  close_target.addEventListener("click", () => set_sidebar(false));
}

document.querySelector("[data_action='print']")?.addEventListener("click", () => window.print());

function normalize_search_text(value) {
  return value.normalize("NFKC").toLocaleLowerCase("zh-CN").replace(/\s+/g, " ").trim();
}

const search_documents = chapter_elements.map((chapter) => {
  const text = (chapter.textContent || "").replace(/\s+/g, " ").trim();
  return {
    id: chapter.id,
    title: chapter.dataset.title || "",
    volume: chapter.dataset.volume || "",
    text,
    search_text: normalize_search_text(text)
  };
});

function create_search_result(document_entry, query) {
  const result_link = document.createElement("a");
  result_link.className = "search_result";
  result_link.href = `#${document_entry.id}`;
  const query_index = document_entry.search_text.indexOf(query);
  const start_index = Math.max(0, query_index - 34);
  const end_index = Math.min(document_entry.text.length, query_index + query.length + 52);
  const snippet = document_entry.text.slice(start_index, end_index);
  const prefix = start_index > 0 ? "…" : "";
  const suffix = end_index < document_entry.text.length ? "…" : "";

  const title_element = document.createElement("strong");
  title_element.textContent = document_entry.title;
  const snippet_element = document.createElement("small");
  snippet_element.textContent = `${prefix}${snippet}${suffix}`;
  result_link.append(title_element, snippet_element);
  result_link.addEventListener("click", () => set_sidebar(false));
  return result_link;
}

function update_search() {
  if (!search_input || !search_results) {
    return;
  }
  const query = normalize_search_text(search_input.value);
  search_results.replaceChildren();
  if (query.length < 2) {
    search_results.dataset.visible = "false";
    return;
  }

  const matches = search_documents.filter((entry) => entry.search_text.includes(query)).slice(0, 24);
  if (matches.length === 0) {
    const empty_result = document.createElement("p");
    empty_result.className = "search_result";
    empty_result.textContent = "本期没有匹配章节";
    search_results.append(empty_result);
  } else {
    for (const match of matches) {
      search_results.append(create_search_result(match, query));
    }
  }
  search_results.dataset.visible = "true";
}

search_input?.addEventListener("input", update_search);
search_input?.addEventListener("keydown", (event) => {
  if (event.key === "Escape") {
    search_input.value = "";
    update_search();
    search_input.blur();
  }
});

function update_progress() {
  if (!progress_bar) {
    return;
  }
  const scrollable_height = document.documentElement.scrollHeight - window.innerHeight;
  const progress = scrollable_height > 0 ? Math.min(1, window.scrollY / scrollable_height) : 0;
  progress_bar.style.width = `${(progress * 100).toFixed(2)}%`;
}

function set_active_chapter(chapter_id) {
  for (const toc_link of toc_links) {
    const is_active = toc_link.getAttribute("href") === `#${chapter_id}`;
    if (is_active) {
      toc_link.setAttribute("aria-current", "true");
      toc_link.closest("details")?.setAttribute("open", "");
    } else {
      toc_link.removeAttribute("aria-current");
    }
  }
  const active_chapter = chapter_elements.find((chapter) => chapter.id === chapter_id);
  if (toolbar_title && active_chapter) {
    toolbar_title.textContent = active_chapter.dataset.title || document.title;
  }
}

const chapter_observer = new IntersectionObserver((entries) => {
  const visible_entries = entries
    .filter((entry) => entry.isIntersecting)
    .sort((left, right) => right.intersectionRatio - left.intersectionRatio);
  if (visible_entries[0]) {
    set_active_chapter(visible_entries[0].target.id);
  }
}, {
  rootMargin: "-18% 0px -67% 0px",
  threshold: [0, 0.05, 0.2]
});

for (const chapter of chapter_elements) {
  chapter_observer.observe(chapter);
}

for (const toc_link of toc_links) {
  toc_link.addEventListener("click", () => set_sidebar(false));
}

for (const toc_section_link of toc_section_links) {
  toc_section_link.addEventListener("click", () => set_sidebar(false));
}

let progress_frame = 0;
window.addEventListener("scroll", () => {
  if (progress_frame) {
    return;
  }
  progress_frame = window.requestAnimationFrame(() => {
    update_progress();
    progress_frame = 0;
  });
}, { passive: true });

document.addEventListener("keydown", (event) => {
  if ((event.ctrlKey || event.metaKey) && event.key.toLocaleLowerCase() === "k") {
    event.preventDefault();
    search_input?.focus();
  }
  if (event.key === "Escape") {
    set_sidebar(false);
  }
});

async function render_mermaid() {
  const mermaid_blocks = document.querySelectorAll("pre.mermaid");
  if (!mermaid_blocks.length || !window.mermaid) {
    return;
  }
  window.mermaid.initialize({
    startOnLoad: false,
    securityLevel: "strict",
    theme: "neutral",
    fontFamily: "Noto Sans SC, Source Han Sans SC, Microsoft YaHei, sans-serif",
    flowchart: { htmlLabels: false, useMaxWidth: true },
    sequence: { useMaxWidth: true, wrap: true }
  });
  try {
    await window.mermaid.run({ nodes: mermaid_blocks });
  } catch (error) {
    console.error("Mermaid rendering failed", error);
  }
}

update_progress();
render_mermaid();

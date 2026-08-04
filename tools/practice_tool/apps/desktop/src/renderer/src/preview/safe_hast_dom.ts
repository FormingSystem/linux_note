import {
  is_safe_hast_root,
  type safe_hast_element,
  type safe_hast_node,
  type safe_hast_root,
} from "@loop/markdown-engine/contracts";

function create_element(node: safe_hast_element): HTMLElement {
  const element = node.tagName === "a" ? document.createElement("span") : document.createElement(node.tagName);
  const properties = node.properties;
  if (node.tagName === "a" && typeof properties.href === "string") {
    element.classList.add("loop_inert_link");
    element.title = `${properties.href}（链接尚未开放）`;
  } else if (node.tagName === "ol" && typeof properties.start === "number") {
    (element as HTMLOListElement).start = properties.start;
  } else if (node.tagName === "input") {
    const input = element as HTMLInputElement;
    input.type = "checkbox";
    input.disabled = true;
    input.checked = properties.checked === true;
  } else if ((node.tagName === "td" || node.tagName === "th") && typeof properties.align === "string") {
    element.dataset.align = properties.align;
  }
  if (Array.isArray(properties.className)) element.classList.add(...properties.className);
  return element;
}

export function render_safe_hast(target: HTMLElement, root: safe_hast_root): void {
  if (!is_safe_hast_root(root)) throw new Error("拒绝无效 safe HAST 块");
  const fragment = document.createDocumentFragment();
  const stack: Array<{ node: safe_hast_node; parent: Node }> = [];
  for (let index = root.children.length - 1; index >= 0; index -= 1) {
    const node = root.children[index];
    if (node) stack.push({ node, parent: fragment });
  }
  while (stack.length > 0) {
    const current = stack.pop();
    if (!current) break;
    if (current.node.type === "text") {
      current.parent.appendChild(document.createTextNode(current.node.value));
      continue;
    }
    const element = create_element(current.node);
    current.parent.appendChild(element);
    for (let index = current.node.children.length - 1; index >= 0; index -= 1) {
      const child = current.node.children[index];
      if (child) stack.push({ node: child, parent: element });
    }
  }
  target.replaceChildren(fragment);
}

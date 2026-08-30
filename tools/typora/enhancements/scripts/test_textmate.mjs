import fs from "node:fs";
import path from "node:path";
import textmate from "vscode-textmate";
import oniguruma from "vscode-oniguruma";

const { Registry, INITIAL, parseRawGrammar } = textmate;
const { loadWASM, OnigScanner, OnigString } = oniguruma;

const root = process.cwd();
await loadWASM(fs.readFileSync(path.join(root, "node_modules/vscode-oniguruma/release/onig.wasm")).buffer);

const files = new Map([
  ["source.c", "c.tmLanguage.json"],
  ["source.cpp", "cpp.tmLanguage.json"],
  ["source.cpp.embedded.macro", "cpp.embedded.macro.tmLanguage.json"],
  ["source.c.platform", "platform.tmLanguage.json"],
]);
const registry = new Registry({
  onigLib: Promise.resolve({
    createOnigScanner: (sources) => new OnigScanner(sources),
    createOnigString: (value) => new OnigString(value),
  }),
  loadGrammar: async (scope_name) => {
    const filename = files.get(scope_name);
    if (!filename) return null;
    const grammar_path = path.join(root, "vendor/vscode_cpp/syntaxes", filename);
    return parseRawGrammar(fs.readFileSync(grammar_path, "utf8"), grammar_path);
  },
});

const grammar = await registry.loadGrammar("source.c");
if (!grammar) throw new Error("source.c grammar failed to load");
const line = "p = rcu_dereference(table[id]);";
const result = grammar.tokenizeLine(line, INITIAL);
const function_token = result.tokens.find((token) => line.slice(token.startIndex, token.endIndex) === "rcu_dereference");
if (!function_token?.scopes.some((scope) => scope.startsWith("entity.name.function"))) {
  throw new Error(`function call was not recognized: ${JSON.stringify(result.tokens)}`);
}
console.log(`recognized rcu_dereference as ${function_token.scopes.at(-1)}`);

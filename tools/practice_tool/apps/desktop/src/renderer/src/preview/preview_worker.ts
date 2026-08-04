/// <reference lib="webworker" />

import {
  PREVIEW_PROTOCOL_VERSION,
  is_preview_source_request,
  parse_markdown_preview,
  preview_engine_error,
  type preview_worker_failure,
  type preview_worker_success,
} from "@loop/markdown-engine";

const worker_scope = self as DedicatedWorkerGlobalScope;

worker_scope.addEventListener("message", (event: MessageEvent<unknown>) => {
  if (!is_preview_source_request(event.data)) {
    worker_scope.postMessage({ message_type: "preview_protocol_error" });
    return;
  }
  const request = event.data;
  try {
    const response: preview_worker_success = {
      message_type: "preview_result",
      document: parse_markdown_preview(request),
    };
    worker_scope.postMessage(response);
  } catch (error) {
    const response: preview_worker_failure = {
      message_type: "preview_failure",
      protocol_version: PREVIEW_PROTOCOL_VERSION,
      document_id: request.document_id,
      revision: request.revision,
      error_code: error instanceof preview_engine_error ? error.error_code : "PREVIEW_PARSE_FAILED",
      user_message: error instanceof preview_engine_error ? error.message : "Markdown 预览解析失败",
    };
    worker_scope.postMessage(response);
  }
});

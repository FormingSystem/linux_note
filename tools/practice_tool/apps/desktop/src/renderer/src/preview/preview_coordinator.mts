import {
  MAXIMUM_PREVIEW_SOURCE_BYTES,
  PREVIEW_PROTOCOL_VERSION,
  is_preview_source_request,
  is_preview_worker_response,
  type preview_document,
  type preview_source_request,
  type preview_worker_failure,
} from "@loop/markdown-engine/contracts";

const PREVIEW_TASK_TIMEOUT_MS = 5_000;

export interface preview_worker_port {
  postMessage(message: unknown): void;
  terminate(): void;
  addEventListener(type: "message", listener: (event: MessageEvent<unknown>) => void): void;
  addEventListener(type: "error" | "messageerror", listener: (event: Event) => void): void;
  removeEventListener(type: "message", listener: (event: MessageEvent<unknown>) => void): void;
  removeEventListener(type: "error" | "messageerror", listener: (event: Event) => void): void;
}

export type preview_worker_factory = () => preview_worker_port;

export interface preview_coordinator_callbacks {
  on_result(document: preview_document): void;
  on_failure(failure: preview_worker_failure): void;
}

function source_byte_length(source: string): number {
  return new TextEncoder().encode(source).byteLength;
}

function failure_for(
  request: preview_source_request,
  error_code: preview_worker_failure["error_code"],
  user_message: string,
): preview_worker_failure {
  return {
    message_type: "preview_failure",
    protocol_version: PREVIEW_PROTOCOL_VERSION,
    document_id: request.document_id,
    revision: request.revision,
    error_code,
    user_message,
  };
}

export class preview_coordinator {
  readonly #worker_factory: preview_worker_factory;
  readonly #callbacks: preview_coordinator_callbacks;
  readonly #latest_revisions = new Map<string, number>();
  #worker: preview_worker_port | null;
  #in_flight: preview_source_request | null = null;
  #pending: preview_source_request | null = null;
  #timeout: ReturnType<typeof setTimeout> | null = null;
  #disposed = false;

  readonly #on_message = (event: MessageEvent<unknown>): void => {
    const request = this.#in_flight;
    if (!request || !is_preview_worker_response(event.data)) {
      this.#restart_after_failure(request, "PREVIEW_PARSE_FAILED", "预览 Worker 返回了无效消息");
      return;
    }
    const response_document_id = event.data.message_type === "preview_result"
      ? event.data.document.document_id
      : event.data.document_id;
    const response_revision = event.data.message_type === "preview_result"
      ? event.data.document.revision
      : event.data.revision;
    if (response_document_id !== request.document_id || response_revision !== request.revision) {
      this.#restart_after_failure(request, "PREVIEW_PARSE_FAILED", "预览 Worker 响应与请求不匹配");
      return;
    }

    this.#finish_in_flight();
    if (this.#latest_revisions.get(response_document_id) === response_revision) {
      if (event.data.message_type === "preview_result") this.#callbacks.on_result(event.data.document);
      else this.#callbacks.on_failure(event.data);
    }
    this.#start_pending();
  };

  readonly #on_worker_error = (): void => {
    if (!this.#in_flight) {
      this.#discard_worker();
      return;
    }
    this.#restart_after_failure(this.#in_flight, "PREVIEW_PARSE_FAILED", "预览 Worker 异常退出");
  };

  constructor(worker_factory: preview_worker_factory, callbacks: preview_coordinator_callbacks) {
    this.#worker_factory = worker_factory;
    this.#callbacks = callbacks;
    this.#worker = this.#create_worker();
  }

  submit(request: preview_source_request): void {
    if (this.#disposed || !is_preview_source_request(request)) {
      throw new Error("拒绝无效或已释放的预览请求");
    }
    const latest_revision = this.#latest_revisions.get(request.document_id);
    if (latest_revision !== undefined && request.revision < latest_revision) return;
    this.#latest_revisions.set(request.document_id, request.revision);
    if (source_byte_length(request.source) > MAXIMUM_PREVIEW_SOURCE_BYTES) {
      this.#callbacks.on_failure(failure_for(request, "PREVIEW_SOURCE_TOO_LARGE", "文档超过 5 MiB 安全预览上限"));
      return;
    }
    if (this.#in_flight) {
      this.#pending = request;
      return;
    }
    this.#start(request);
  }

  clear_document(document_id: string): void {
    this.#latest_revisions.delete(document_id);
    if (this.#pending?.document_id === document_id) this.#pending = null;
  }

  clear_all(): void {
    this.#latest_revisions.clear();
    this.#pending = null;
    if (this.#in_flight) {
      this.#finish_in_flight();
      this.#replace_worker();
    }
  }

  dispose(): void {
    if (this.#disposed) return;
    this.#disposed = true;
    this.#latest_revisions.clear();
    this.#pending = null;
    this.#finish_in_flight();
    this.#discard_worker();
  }

  #create_worker(): preview_worker_port {
    const worker = this.#worker_factory();
    worker.addEventListener("message", this.#on_message);
    worker.addEventListener("error", this.#on_worker_error);
    worker.addEventListener("messageerror", this.#on_worker_error);
    return worker;
  }

  #detach_worker(worker: preview_worker_port): void {
    worker.removeEventListener("message", this.#on_message);
    worker.removeEventListener("error", this.#on_worker_error);
    worker.removeEventListener("messageerror", this.#on_worker_error);
  }

  #replace_worker(): void {
    this.#discard_worker();
    if (!this.#disposed) this.#worker = this.#create_worker();
  }

  #discard_worker(): void {
    const worker = this.#worker;
    this.#worker = null;
    if (!worker) return;
    this.#detach_worker(worker);
    worker.terminate();
  }

  #start(request: preview_source_request): void {
    this.#in_flight = request;
    if (!this.#worker) this.#worker = this.#create_worker();
    try {
      this.#worker.postMessage(request);
    } catch {
      this.#restart_after_failure(request, "PREVIEW_PARSE_FAILED", "无法向预览 Worker 投递任务");
      return;
    }
    this.#timeout = setTimeout(() => {
      this.#restart_after_failure(request, "PREVIEW_PARSE_FAILED", "预览任务超过 5 秒，已终止隔离 Worker");
    }, PREVIEW_TASK_TIMEOUT_MS);
  }

  #finish_in_flight(): void {
    if (this.#timeout) clearTimeout(this.#timeout);
    this.#timeout = null;
    this.#in_flight = null;
  }

  #restart_after_failure(
    request: preview_source_request | null,
    error_code: preview_worker_failure["error_code"],
    user_message: string,
  ): void {
    if (this.#disposed) return;
    this.#finish_in_flight();
    this.#replace_worker();
    if (request && this.#latest_revisions.get(request.document_id) === request.revision) {
      this.#callbacks.on_failure(failure_for(request, error_code, user_message));
    }
    this.#start_pending();
  }

  #start_pending(): void {
    const pending = this.#pending;
    this.#pending = null;
    if (pending && this.#latest_revisions.get(pending.document_id) === pending.revision) this.#start(pending);
  }
}

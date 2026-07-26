const DATABASE_NAME = "loop-knowledge-practice";
const DATABASE_VERSION = 2;

export const STORES = {
  workspace: "workspace",
  sessions: "sessions",
  importedBooks: "importedBooks",
} as const;

let databasePromise: Promise<IDBDatabase> | null = null;

export function openDatabase(): Promise<IDBDatabase> {
  if (databasePromise) return databasePromise;
  databasePromise = new Promise((resolve, reject) => {
    const request = indexedDB.open(DATABASE_NAME, DATABASE_VERSION);
    request.onupgradeneeded = () => {
      const database = request.result;
      if (!database.objectStoreNames.contains(STORES.workspace)) {
        database.createObjectStore(STORES.workspace);
      }
      if (!database.objectStoreNames.contains(STORES.sessions)) {
        const sessions = database.createObjectStore(STORES.sessions, { keyPath: "id" });
        sessions.createIndex("unitId", "unitId");
        sessions.createIndex("updatedAt", "updatedAt");
      }
      if (!database.objectStoreNames.contains(STORES.importedBooks)) {
        const books = database.createObjectStore(STORES.importedBooks, { keyPath: "id" });
        books.createIndex("status", "status");
        books.createIndex("updatedAt", "updatedAt");
      }
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error ?? new Error("无法打开本地数据库"));
  });
  return databasePromise;
}

export async function readRecord<T>(storeName: string, key: IDBValidKey): Promise<T | undefined> {
  const database = await openDatabase();
  return new Promise((resolve, reject) => {
    const transaction = database.transaction(storeName, "readonly");
    const request = transaction.objectStore(storeName).get(key);
    request.onsuccess = () => resolve(request.result as T | undefined);
    request.onerror = () => reject(request.error ?? new Error("读取本地数据失败"));
  });
}

export async function writeRecord<T>(storeName: string, value: T, key?: IDBValidKey): Promise<void> {
  const database = await openDatabase();
  return new Promise((resolve, reject) => {
    const transaction = database.transaction(storeName, "readwrite");
    const store = transaction.objectStore(storeName);
    if (key === undefined) store.put(value);
    else store.put(value, key);
    transaction.oncomplete = () => resolve();
    transaction.onerror = () => reject(transaction.error ?? new Error("保存本地数据失败"));
    transaction.onabort = () => reject(transaction.error ?? new Error("保存本地数据已中止"));
  });
}

export async function readAllRecords<T>(storeName: string): Promise<T[]> {
  const database = await openDatabase();
  return new Promise((resolve, reject) => {
    const transaction = database.transaction(storeName, "readonly");
    const request = transaction.objectStore(storeName).getAll();
    request.onsuccess = () => resolve(request.result as T[]);
    request.onerror = () => reject(request.error ?? new Error("读取本地数据失败"));
  });
}

export async function deleteRecord(storeName: string, key: IDBValidKey): Promise<void> {
  const database = await openDatabase();
  return new Promise((resolve, reject) => {
    const transaction = database.transaction(storeName, "readwrite");
    transaction.objectStore(storeName).delete(key);
    transaction.oncomplete = () => resolve();
    transaction.onerror = () => reject(transaction.error ?? new Error("删除本地数据失败"));
    transaction.onabort = () => reject(transaction.error ?? new Error("删除本地数据已中止"));
  });
}

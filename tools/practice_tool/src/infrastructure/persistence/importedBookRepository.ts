import type { ImportedBook } from "../../shared/types";
import { deleteRecord, readAllRecords, readRecord, STORES, writeRecord } from "./database";

export function listImportedBooks(): Promise<ImportedBook[]> {
  return readAllRecords<ImportedBook>(STORES.importedBooks).then((books) =>
    books.sort((left, right) => right.updatedAt.localeCompare(left.updatedAt)),
  );
}

export function loadImportedBook(id: string): Promise<ImportedBook | undefined> {
  return readRecord<ImportedBook>(STORES.importedBooks, id);
}

export function saveImportedBook(book: ImportedBook): Promise<void> {
  return writeRecord(STORES.importedBooks, book);
}

export function deleteImportedBook(id: string): Promise<void> {
  return deleteRecord(STORES.importedBooks, id);
}

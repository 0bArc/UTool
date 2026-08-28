const PAGE_SIZE = 25;

export { PAGE_SIZE };

export function pageCount(total: number, pageSize = PAGE_SIZE): number {
  return Math.max(1, Math.ceil(total / pageSize));
}

export function slicePage<T>(items: T[], page: number, pageSize = PAGE_SIZE): T[] {
  const start = page * pageSize;
  return items.slice(start, start + pageSize);
}

/** Items from the start through the end of `page` (for infinite scroll). */
export function sliceThroughPage<T>(items: T[], page: number, pageSize = PAGE_SIZE): T[] {
  return items.slice(0, (page + 1) * pageSize);
}

export function pageWindow(current: number, total: number, max = 7): number[] {
  if (total <= max) return Array.from({ length: total }, (_, i) => i);
  const half = Math.floor(max / 2);
  let start = Math.max(0, current - half);
  const end = Math.min(total - 1, start + max - 1);
  start = Math.max(0, end - max + 1);
  return Array.from({ length: end - start + 1 }, (_, i) => start + i);
}

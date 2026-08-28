"use client";

type Props = {
  page: number;
  totalPages: number;
  totalItems: number;
  shownCount: number;
  onPageChange: (page: number) => void;
};

export function Paginator({
  page,
  totalPages,
  totalItems,
  shownCount,
  onPageChange,
}: Props) {
  if (totalItems === 0) return null;

  const hasMore = page < totalPages - 1;

  return (
    <div className="paginator">
      <span className="paginator-range">
        {shownCount.toLocaleString()} of {totalItems.toLocaleString()}
        {hasMore ? ` · page ${page + 1}/${totalPages}` : ""}
      </span>
      <div className="paginator-controls">
        <button type="button" className="pag-btn" disabled={page <= 0} onClick={() => onPageChange(page - 1)}>
          Prev
        </button>
        <button
          type="button"
          className="pag-btn"
          disabled={!hasMore}
          onClick={() => onPageChange(Math.min(totalPages - 1, page + 1))}
        >
          Next
        </button>
      </div>
    </div>
  );
}

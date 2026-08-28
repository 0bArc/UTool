"use client";

import { useEffect, useRef, useState } from "react";
import { buildPathSuggestions } from "@/lib/pak-tree";
import type { PakEntry } from "@/lib/types";

type Props = {
  value: string;
  onChange: (value: string) => void;
  onPick?: (path: string) => void;
  onSubmit: () => void;
  entries: PakEntry[];
  disabled?: boolean;
};

export function PathSearch({
  value,
  onChange,
  onPick,
  onSubmit,
  entries,
  disabled,
}: Props) {
  const [open, setOpen] = useState(false);
  const [activeIndex, setActiveIndex] = useState(0);
  const wrapRef = useRef<HTMLDivElement>(null);

  const suggestions = buildPathSuggestions(entries, value);

  useEffect(() => {
    setActiveIndex(0);
  }, [value, suggestions.length]);

  useEffect(() => {
    const onDocClick = (e: MouseEvent) => {
      if (!wrapRef.current?.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener("mousedown", onDocClick);
    return () => document.removeEventListener("mousedown", onDocClick);
  }, []);

  const pick = (path: string) => {
    onChange(path);
    onPick?.(path);
    setOpen(false);
  };

  const onKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      if (open && suggestions[activeIndex]) {
        e.preventDefault();
        pick(suggestions[activeIndex]);
        return;
      }
      void onSubmit();
      setOpen(false);
      return;
    }
    if (!open || suggestions.length === 0) return;
    if (e.key === "ArrowDown") {
      e.preventDefault();
      setActiveIndex((i) => Math.min(i + 1, suggestions.length - 1));
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      setActiveIndex((i) => Math.max(i - 1, 0));
    } else if (e.key === "Escape") {
      setOpen(false);
    }
  };

  return (
    <div className="search-wrap" ref={wrapRef}>
      <input
        className="search-input"
        value={value}
        onChange={(e) => {
          onChange(e.target.value);
          setOpen(true);
        }}
        onFocus={() => setOpen(true)}
        onKeyDown={onKeyDown}
        placeholder="Search assets…"
        spellCheck={false}
        disabled={disabled}
        autoComplete="off"
        role="combobox"
        aria-expanded={open && suggestions.length > 0}
        aria-autocomplete="list"
      />
      {open && suggestions.length > 0 && value.trim() ? (
        <ul className="search-suggest" role="listbox">
          {suggestions.map((path, i) => (
            <li key={path}>
              <button
                type="button"
                role="option"
                aria-selected={i === activeIndex}
                className={`search-suggest-item ${i === activeIndex ? "search-suggest-active" : ""}`}
                onMouseDown={(e) => e.preventDefault()}
                onClick={() => pick(path)}
              >
                {path}
              </button>
            </li>
          ))}
        </ul>
      ) : null}
    </div>
  );
}

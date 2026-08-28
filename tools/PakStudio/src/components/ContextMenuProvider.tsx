"use client";

import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";
import { ContextMenu, type ContextMenuItem } from "@/components/ContextMenu";

type OpenArgs = {
  x: number;
  y: number;
  items: ContextMenuItem[];
  onSelect: (id: string) => void;
};

type ContextMenuApi = {
  open: (e: React.MouseEvent | MouseEvent, items: ContextMenuItem[], onSelect: (id: string) => void) => void;
  close: () => void;
};

const ContextMenuCtx = createContext<ContextMenuApi | null>(null);

export function useContextMenu(): ContextMenuApi {
  const ctx = useContext(ContextMenuCtx);
  if (!ctx) throw new Error("useContextMenu requires ContextMenuProvider");
  return ctx;
}

export function ContextMenuProvider({ children }: { children: ReactNode }) {
  const [menu, setMenu] = useState<OpenArgs | null>(null);

  const close = useCallback(() => setMenu(null), []);

  const open = useCallback(
    (e: React.MouseEvent | MouseEvent, items: ContextMenuItem[], onSelect: (id: string) => void) => {
      e.preventDefault();
      e.stopPropagation();
      if (items.length === 0) {
        setMenu(null);
        return;
      }
      setMenu({
        x: e.clientX,
        y: e.clientY,
        items,
        onSelect,
      });
    },
    [],
  );

  useEffect(() => {
    const blockBrowserMenu = (e: Event) => {
      e.preventDefault();
    };
    document.addEventListener("contextmenu", blockBrowserMenu);
    return () => document.removeEventListener("contextmenu", blockBrowserMenu);
  }, []);

  const api = useMemo(() => ({ open, close }), [open, close]);

  return (
    <ContextMenuCtx.Provider value={api}>
      {children}
      {menu ? (
        <ContextMenu
          x={menu.x}
          y={menu.y}
          items={menu.items}
          onClose={close}
          onSelect={(id) => {
            menu.onSelect(id);
            close();
          }}
        />
      ) : null}
    </ContextMenuCtx.Provider>
  );
}

export type { ContextMenuItem };

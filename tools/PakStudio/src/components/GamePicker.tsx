"use client";

import { useCallback, useEffect, useState } from "react";
import { fetchGames, probeGamePath, type ConfigGame } from "@/lib/games-client";

type Props = {
  source: string;
  onSourceChange: (source: string) => void;
  onSourceReady?: (source: string) => void;
  disabled?: boolean;
};

export function GamePicker({ source, onSourceChange, onSourceReady, disabled }: Props) {
  const [games, setGames] = useState<ConfigGame[]>([]);
  const [pathInput, setPathInput] = useState("");
  const [scanStatus, setScanStatus] = useState("");
  const [busy, setBusy] = useState(false);
  const [picked, setPicked] = useState(false);

  const loadGames = useCallback(async () => {
    const doc = await fetchGames();
    setGames(doc.games ?? []);
    if (!picked && doc.games?.length) {
      onSourceChange(doc.games[0].id);
    }
  }, [onSourceChange, picked]);

  useEffect(() => {
    void loadGames();
  }, [loadGames]);

  const scanPath = async () => {
    const path = pathInput.trim();
    if (!path) return;
    setBusy(true);
    setScanStatus("Scanning");
    try {
      const doc = await probeGamePath(path);
      if (!doc.ok || !doc.ready || !doc.source) {
        setScanStatus(doc.error ?? "No paks found");
        return;
      }
      setPicked(true);
      onSourceChange(doc.source);
      onSourceReady?.(doc.source);
      setScanStatus(
        doc.matchedGameId
          ? `${doc.matchedGameId}, ${doc.pakCount ?? 0} paks`
          : `${doc.pakCount ?? 0} paks`,
      );
    } catch (err) {
      setScanStatus(String(err));
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="game-picker">
      {games.length > 0 ? (
        <div className="game-row">
          {games.map((g) => (
            <button
              key={g.id}
              type="button"
              className={`game-btn ${source === g.id ? "game-btn-active" : ""}`}
              disabled={disabled || busy}
              onClick={() => {
                setPicked(true);
                onSourceChange(g.id);
                setScanStatus("");
                onSourceReady?.(g.id);
              }}
              title={g.paksDir}
            >
              {g.id}
              {g.pakCount != null ? ` (${g.pakCount})` : ""}
            </button>
          ))}
        </div>
      ) : (
        <p className="game-note">No games in utool.json. Scan an install folder.</p>
      )}

      <div className="game-scan">
        <input
          className="control-input"
          value={pathInput}
          onChange={(e) => setPathInput(e.target.value)}
          placeholder="D:\Steam\steamapps\common\Pacific Drive"
          spellCheck={false}
          disabled={disabled || busy}
          onKeyDown={(e) => {
            if (e.key === "Enter") void scanPath();
          }}
        />
        <button type="button" className="btn" disabled={disabled || busy} onClick={() => void scanPath()}>
          Scan
        </button>
      </div>
      {scanStatus ? <p className="game-scan-status">{scanStatus}</p> : null}
    </div>
  );
}

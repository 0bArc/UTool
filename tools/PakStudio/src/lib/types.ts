export type PakEntry = {
  sourcePak: string;
  virtualPath: string;
  size: number;
  extension: string;
};

export type UtoolListResponse = {
  entries?: PakEntry[];
  total?: number;
  fromCache?: boolean;
};

export type UtoolPreviewResponse = {
  kind?: string;
  payload?: unknown;
  virtualPath?: string;
  sourcePak?: string;
};

export type UtoolSnippetResponse = {
  snippet?: string;
  kind?: string;
  virtualPath?: string;
  sourcePak?: string;
};

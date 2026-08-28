import type { AppProps } from "next/app";
import { ContextMenuProvider } from "@/components/ContextMenuProvider";
import "./globals.css";

export default function App({ Component, pageProps }: AppProps) {
  return (
    <ContextMenuProvider>
      <Component {...pageProps} />
    </ContextMenuProvider>
  );
}

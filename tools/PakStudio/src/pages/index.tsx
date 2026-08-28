import Head from "next/head";
import { PakExplorer } from "@/components/PakExplorer";

export default function HomePage() {
  return (
    <>
      <Head>
        <title>UTool Studio</title>
        <meta name="viewport" content="width=device-width, initial-scale=1" />
      </Head>
      <PakExplorer />
    </>
  );
}

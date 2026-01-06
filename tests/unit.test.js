import { afterAll, describe, it } from "vitest";
import { execSync } from "node:child_process";
import path from "node:path";

const ROOT = path.resolve(__dirname, "../src/lib");

describe("C unit tests", () => {
  afterAll(() => {
    execSync(
      "rm ./*.out",
      {
        cwd: ROOT,
        stdio: "inherit"
      }
    );
    console.log("Deleted test binaries");
  });

  it("cfns.c", () => {
    // compile
    execSync(
      "gcc cfns.test.c cfns.c -lcriterion -o cfns.test.out",
      {
        cwd: ROOT,
        stdio: "inherit", // show compiler output
      }
    );

    // run
    execSync(
      "./cfns.test.out --verbose",
      {
        cwd: ROOT,
        stdio: "inherit", // show Criterion output
      }
    );
  });
});


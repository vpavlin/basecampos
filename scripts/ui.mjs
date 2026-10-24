// Drive Basecamp in the VM through its QML inspector (logos-qt-mcp protocol).
//   node ui.mjs <command> [json-params]      e.g. findAndClick '{"text":"System"}'
//   node ui.mjs screenshot out.png
// Talks to 127.0.0.1:$UI_PORT (scripts/ui.sh opens the ssh tunnel).
import net from "node:net";
import fs from "node:fs";

const [command, arg] = process.argv.slice(2);
const port = Number(process.env.UI_PORT || 13768);
const params = command === "screenshot" || !arg ? {} : JSON.parse(arg);

const sock = net.createConnection({ host: "127.0.0.1", port });
let buf = "";
sock.on("connect", () => sock.write(JSON.stringify({ id: 1, command, params }) + "\n"));
sock.on("data", (c) => {
  buf += c.toString("utf-8");
  const i = buf.indexOf("\n");
  if (i < 0) return;
  const msg = JSON.parse(buf.slice(0, i));
  sock.destroy();
  if (command === "screenshot" && msg.image) {
    fs.writeFileSync(arg || "shot.png", Buffer.from(msg.image, "base64"));
    console.log(arg || "shot.png", msg.width + "x" + msg.height);
  } else {
    console.log(JSON.stringify(msg, null, 1));
  }
  process.exit(msg.error ? 1 : 0);
});
sock.on("error", (e) => { console.error(e.message); process.exit(1); });
setTimeout(() => { console.error("timeout"); process.exit(1); }, 20000);

// import { invoke } from "@tauri-apps/api/core";

// let greetInputEl: HTMLInputElement | null;
// let greetMsgEl: HTMLElement | null;

// async function greet() {
//   if (greetMsgEl && greetInputEl) {
//     // Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
//     greetMsgEl.textContent = await invoke("greet", {
//       name: greetInputEl.value,
//     });
//   }
// }

// window.addEventListener("DOMContentLoaded", () => {
//   greetInputEl = document.querySelector("#greet-input");
//   greetMsgEl = document.querySelector("#greet-msg");
//   document.querySelector("#greet-form")?.addEventListener("submit", (e) => {
//     e.preventDefault();
//     greet();
//   });
// });

import { invoke } from "@tauri-apps/api/core";

let scanBtnEl: HTMLButtonElement | null = null;
let resultMsgEl: HTMLElement | null = null;

window.addEventListener("DOMContentLoaded", async () => {
  console.log("DOM 로드 완료");

  scanBtnEl = document.querySelector("#scan-btn");
  resultMsgEl = document.querySelector(".result-msg");

  console.log("scanBtnEl:", scanBtnEl); // 요소가 제대로 잡혔는지 확인
  console.log("resultMsgEl:", resultMsgEl);

  if (scanBtnEl) {
    scanBtnEl.addEventListener("click", () => {
      console.log("click 이벤트 발생!");
      alert("버튼 클릭됨");
    });
  } else {
    console.error("버튼을 찾을 수 없습니다.");
  }
});

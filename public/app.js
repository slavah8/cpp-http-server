const button = document.getElementById("ping-button");
const message = document.getElementById("message");

if (button && message) {
    button.addEventListener("click", () => {
        message.textContent = "JavaScript loaded successfully.";
    });
}

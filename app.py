import tkinter as tk
import subprocess
import secrets
import random
from tkinterdnd2 import TkinterDnD, DND_FILES
from tkinter import filedialog

root = TkinterDnD.Tk()
root.title("Encode Password")
root.geometry("1024x576")

def on_drop(event, entry_field):
    entry_field.delete(0, tk.END)
    entry_field.insert(0, event.data.replace('{', '').replace('}', ''))

def browse_file(entry_field):
    file_path = filedialog.askopenfilename(
        filetypes=[("Image Files", "*.png")]
    )
    if file_path:
        entry_field.delete(0, tk.END)
        entry_field.insert(0, file_path)

def pack_encode_plaintext():
    aes_key_label.pack_forget()
    aes_key_entry.pack_forget()
    aes_key_browse.pack_forget()

    message_label.pack(pady=5)
    message_entry.pack(pady=5)
    message_visibility_button.pack_forget()
    message_visibility_button.pack(pady=5)

    generate_password_button.pack_forget()
    generate_password_button.pack(pady=5)

    output_img_label.pack_forget()
    output_img_entry.pack_forget()
    output_img_label.pack(pady=5)
    output_img_entry.pack(pady=5)

    submit_button.pack_forget()
    submit_button.pack(pady=10)

    status_label.pack_forget()
    status_label.pack(pady=10)

def pack_encode_aes():
    aes_key_label.pack(pady=5)
    aes_key_entry.pack(pady=5)
    aes_key_browse.pack(pady=5)

    message_label.pack_forget()
    message_entry.pack_forget()
    message_visibility_button.pack_forget()
    message_label.pack(pady=5)
    message_entry.pack(pady=5)
    message_visibility_button.pack(pady=5)

    generate_password_button.pack_forget()
    generate_password_button.pack(pady=5)

    output_img_label.pack_forget()
    output_img_entry.pack_forget()
    output_img_label.pack(pady=5)
    output_img_entry.pack(pady=5)

    submit_button.pack_forget()
    submit_button.pack(pady=10)

    status_label.pack_forget()
    status_label.pack(pady=10)

def pack_decode_plaintext():
    aes_key_label.pack_forget()
    aes_key_entry.pack_forget()
    aes_key_browse.pack_forget()

    submit_button.pack_forget()
    submit_button.pack(pady=10)

    status_label.pack_forget()
    status_label.pack(pady=10)
    
def pack_decode_aes():
    aes_key_label.pack(pady=5)
    aes_key_entry.pack(pady=5)
    aes_key_browse.pack(pady=5)
    
    submit_button.pack_forget()
    submit_button.pack(pady=10)

    status_label.pack_forget()
    status_label.pack(pady=10)

def toggle_mode():
    decoded_output_title_label.pack_forget()
    decoded_output_entry.pack_forget()
    toggle_output_button.pack_forget()
    copy_button.pack_forget()

    status_label.config(text="")

    if operating_mode.get() == "Decode":
        main_img_label.config(text="Enter image path to decrypt: ")
        output_img_label.pack_forget()
        output_img_entry.pack_forget()
        message_label.pack_forget()
        message_entry.pack_forget()
        message_visibility_button.pack_forget()
        generate_password_button.pack_forget()
        
        if encryption_mode.get() == "Plaintext":
            pack_decode_plaintext()
        elif encryption_mode.get() == "AES":
            pack_decode_aes()
        
    elif operating_mode.get() == "Encode":
        main_img_label.config(text="Enter image path to embed a message into:")
        
        if encryption_mode.get() == "Plaintext":
            pack_encode_plaintext()
        elif encryption_mode.get() == "AES":
            pack_encode_aes()

def toggle_msg_visibility():
    if message_entry.cget("show") == "":
        message_entry.config(show="*")
        message_visibility_button.config(text="Show Message")
    else:
        message_entry.config(show="")
        message_visibility_button.config(text="Hide Message")

def generate_random_password():
    random_password_length = random.randint(20, 30)
    random_password = secrets.token_urlsafe(random_password_length)
    message_entry.delete(0, tk.END)
    message_entry.insert(0, random_password)

def submit():
    if operating_mode.get() == "Encode":
        if encryption_mode.get() == "Plaintext":
            result = subprocess.run(["./stegopng", "0", "0", main_img_entry.get(), message_entry.get(), output_img_entry.get() + ".png"], capture_output=True, text=True)
        elif encryption_mode.get() == "AES":
            outputFile = output_img_entry.get()
            outputKeyFile = outputFile + ' - key'

            result = subprocess.run(["./stegopng", "0", "1", main_img_entry.get(), message_entry.get(), outputFile + ".png", aes_key_entry.get(), outputKeyFile + ".png"], capture_output=True, text=True)
        
        if len(result.stderr) == 0:
            status_label.config(text="Success!")
        else:
            status_label.config(text=result.stderr)

            
    elif operating_mode.get() == "Decode":
        if encryption_mode.get() == "Plaintext":
            result = subprocess.run(["./stegopng", "1", "0", main_img_entry.get()], capture_output=True)
        elif encryption_mode.get() == "AES":
            result = subprocess.run(["./stegopng", "1", "1", main_img_entry.get(), aes_key_entry.get()], capture_output=True)
        
        if len(result.stderr) == 0:
            status_label.config(text="Success!")
            decoded_output_entry.delete(0, tk.END)
            decoded_output_entry.insert(0, result.stdout)
            decoded_output_title_label.pack(pady=5)
            decoded_output_entry.pack(pady=5)

            toggle_output_button.pack(pady=5)
            copy_button.pack(pady=5)
        else:
            status_label.config(text=result.stderr)

    main_img_entry.delete(0, tk.END)
    message_entry.delete(0, tk.END)
    aes_key_entry.delete(0, tk.END)
    output_img_entry.delete(0, tk.END)

def toggle_decoded_output_visibility():
    if decoded_output_entry.cget("show") == "":
        decoded_output_entry.config(show="*")
        toggle_output_button.config(text="Show Output")
    else:
        decoded_output_entry.config(show="")
        toggle_output_button.config(text="Hide Output")

def copy_to_clipboard():
    root.clipboard_clear()
    root.clipboard_append(decoded_output_entry.get())
    root.update()

decoded_output = ""

operating_mode = tk.StringVar(value="Encode")
encode_radio = tk.Radiobutton(root, text="Encode Mode", variable=operating_mode, value="Encode", command=toggle_mode)
decode_radio = tk.Radiobutton(root, text="Decode Mode", variable=operating_mode, value="Decode", command=toggle_mode)
encode_radio.pack(pady=5)
decode_radio.pack(pady=5)

main_img_label = tk.Label(root, text="Enter image path to embed a message into:")
main_img_label.pack(pady=10)

main_img_entry = tk.Entry(root, width=50)
main_img_entry.pack(pady=5)

main_img_entry.drop_target_register(DND_FILES)
main_img_entry.dnd_bind('<<Drop>>', lambda event: on_drop(event, main_img_entry))

browse_button = tk.Button(root, text="Browse", command=lambda: browse_file(main_img_entry))
browse_button.pack(pady=5)

encryption_mode = tk.StringVar(value="Plaintext")
plaintext_radio = tk.Radiobutton(root, text="Plaintext Mode", variable=encryption_mode, value="Plaintext", command=toggle_mode)
aes_radio = tk.Radiobutton(root, text="AES Encrypted Mode", variable=encryption_mode, value="AES", command=toggle_mode)
plaintext_radio.pack(pady=5)
aes_radio.pack(pady=5)

aes_key_label = tk.Label(root, text="Enter image path to use as the key:")
aes_key_entry = tk.Entry(root, width=50)
aes_key_browse = tk.Button(root, text="Browse", command=lambda: browse_file(aes_key_entry))

aes_key_entry.drop_target_register(DND_FILES)
aes_key_entry.dnd_bind('<<Drop>>', lambda event: on_drop(event, aes_key_entry))

message_label = tk.Label(root, text="Message:")
message_entry = tk.Entry(root, width=50, show="*")
message_label.pack(pady=5)
message_entry.pack(pady=5)

generate_password_button = tk.Button(root, text="Generate Password", command=generate_random_password)
generate_password_button.pack(pady=5)

message_visibility_button = tk.Button(root, text="Show Message", command=toggle_msg_visibility)
message_visibility_button.pack(pady=5)

output_img_label = tk.Label(root, text="Output image name (without file extension):")
output_img_entry = tk.Entry(root, width=50)
output_img_label.pack(pady=5)
output_img_entry.pack(pady=5)

submit_button = tk.Button(root, text="Submit", command=submit)
submit_button.pack(pady=10)

status_label = tk.Label(root, text="", font=("Arial", 12))
status_label.pack(pady=10)

decoded_output_title_label = tk.Label(root, text="Decoded message:")
decoded_output_entry = tk.Entry(root, width=50, show='*')

toggle_output_button = tk.Button(root, text="Show Output", command=toggle_decoded_output_visibility)
copy_button = tk.Button(root, text="Copy to Clipboard", command=copy_to_clipboard)

root.mainloop()
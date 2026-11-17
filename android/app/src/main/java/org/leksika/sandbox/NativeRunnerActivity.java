package org.leksika.sandbox;

import android.app.Activity;
import android.graphics.Typeface;
import android.os.Bundle;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.zip.GZIPInputStream;

/**
 * Полный исполнитель Лексуса на телефоне.
 *
 * Приложение запускает нативное ядро (тот же C11-исходник, что и на компьютере)
 * в отдельном процессе, отдаёт ему файл программы и показывает всё, что ядро
 * напечатало. Ни Python, ни сеть для этого не нужны.
 *
 * Мост проверки: intent с флагом «автозапуск» сразу выполняет программу из
 * поля правки. Так эмулятор проверяет работу ядра, не разыскивая кнопку по
 * координатам.
 */
public final class NativeRunnerActivity extends Activity {
    /** Имя файла ядра внутри APK: lib/arm64-v8a/libleksus-core.so. */
    private static final String CORE_NAME = "libleksus-core.so";
    /** Словарь форм лежит в APK сжатым, рядом с ядром ему нужен обычный файл. */
    private static final String DICTIONARY_ASSET = "dictionary.lxd.gz";
    private static final String DICTIONARY_NAME = "dictionary.lxd";
    /** Программа по умолчанию показывает новое: словари. */
    private static final String SAMPLE =
            "пусть цены равно словарь().\n"
            + "записать в цены по ключу хлеб тридцать.\n"
            + "записать в цены по ключу молоко сорок.\n"
            + "сказать речь Позиций: (длина цен) конец речи.\n"
            + "сказать ключи(цены).\n"
            + "сказать прочитать из цен по ключу хлеб.\n";
    private static final long DICTIONARY_MIN_BYTES = 1024L * 1024L;
    private static final long RUN_TIMEOUT_MILLIS = 15000L;

    private EditText editor;
    private TextView output;
    private Button runButton;
    private File dictionary;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setTitle("Лексика — нативное ядро");
        dictionary = new File(getFilesDir(), DICTIONARY_NAME);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(8);
        root.setPadding(pad, pad, pad, pad);

        TextView header = new TextView(this);
        header.setText("Программа на Лексусе выполняется нативным ядром на этом устройстве.");
        header.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        root.addView(header, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        editor = new EditText(this);
        editor.setGravity(Gravity.TOP | Gravity.START);
        editor.setTypeface(Typeface.MONOSPACE);
        editor.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        editor.setText(SAMPLE);
        LinearLayout.LayoutParams editorParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(200));
        root.addView(editor, editorParams);

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        runButton = new Button(this);
        runButton.setText("Выполнить");
        runButton.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { runProgram(); }
        });
        buttons.addView(runButton, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        Button sandbox = new Button(this);
        sandbox.setText("Песочница");
        sandbox.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                startActivity(new android.content.Intent(
                        NativeRunnerActivity.this, MainActivity.class));
            }
        });
        buttons.addView(sandbox, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        root.addView(buttons, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        output = new TextView(this);
        output.setTypeface(Typeface.MONOSPACE);
        output.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        output.setTextIsSelectable(true);
        output.setText("Готово к запуску.");
        ScrollView scroll = new ScrollView(this);
        scroll.addView(output, new android.widget.FrameLayout.LayoutParams(
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                android.widget.FrameLayout.LayoutParams.WRAP_CONTENT));
        root.addView(scroll, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));

        setContentView(root);

        String program = getIntent().getStringExtra("программа");
        if (program != null && program.trim().length() > 0) editor.setText(program);
        if (getIntent().getBooleanExtra("автозапуск", false)) {
            editor.postDelayed(new Runnable() {
                @Override public void run() { runProgram(); }
            }, 300);
        }
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
                getResources().getDisplayMetrics());
    }

    /** Запуск в отдельном потоке: ядро — внешний процесс, ждать его в UI нельзя. */
    private void runProgram() {
        final String source = editor.getText().toString();
        runButton.setEnabled(false);
        output.setText("Выполняется…");
        Thread worker = new Thread(new Runnable() {
            @Override public void run() {
                final String text = execute(source);
                runOnUiThread(new Runnable() {
                    @Override public void run() {
                        output.setText(text);
                        runButton.setEnabled(true);
                    }
                });
            }
        }, "leksika-runner");
        worker.setDaemon(true);
        worker.start();
    }

    private String execute(String source) {
        String libraryDir = getApplicationInfo().nativeLibraryDir;
        File core = libraryDir == null ? new File("/nonexistent") : new File(libraryDir, CORE_NAME);
        if (!core.isFile()) {
            return "В этой сборке нет нативного ядра arm64.\n"
                 + "Откройте песочницу: она работает на JavaScript-машине.";
        }
        File dir = new File(getFilesDir(), "программы");
        if (!dir.isDirectory() && !dir.mkdirs()) return "Не удалось создать каталог программ.";
        File program = new File(dir, "программа.lek");
        try {
            writeText(program, source);
        } catch (IOException error) {
            return "Не удалось сохранить программу: " + error.getMessage();
        }
        String dictionaryNote = prepareDictionary();

        ProcessBuilder builder = new ProcessBuilder(
                core.getAbsolutePath(), "выполнить", program.getAbsolutePath());
        builder.directory(dir);
        builder.redirectErrorStream(true);
        if (dictionary.isFile()) {
            builder.environment().put("LEKSIKA_DICTIONARY", dictionary.getAbsolutePath());
        }
        Process process;
        try {
            process = builder.start();
        } catch (IOException error) {
            return "Не удалось запустить ядро: " + error.getMessage();
        }
        // Сторож снимает зависшую программу: бесконечный цикл не должен
        // оставлять приложение без ответа.
        final Process started = process;
        Thread guard = new Thread(new Runnable() {
            @Override public void run() {
                try {
                    Thread.sleep(RUN_TIMEOUT_MILLIS);
                } catch (InterruptedException stopped) {
                    return;
                }
                started.destroy();
            }
        }, "leksika-guard");
        guard.setDaemon(true);
        guard.start();

        String printed;
        try {
            printed = readText(process.getInputStream());
        } catch (IOException error) {
            process.destroy();
            return "Не удалось прочитать вывод ядра: " + error.getMessage();
        }
        int code;
        try {
            code = process.waitFor();
        } catch (InterruptedException stopped) {
            process.destroy();
            Thread.currentThread().interrupt();
            return "Выполнение прервано.";
        }
        StringBuilder answer = new StringBuilder();
        if (code == 0) answer.append("Выполнено.");
        else answer.append("Ядро завершилось с кодом ").append(code).append('.');
        answer.append('\n');
        if (printed.length() > 0) answer.append(printed);
        if (dictionaryNote.length() > 0) answer.append('\n').append(dictionaryNote);
        return answer.toString();
    }

    /**
     * Разворачивает словарь форм при первом запуске. Без него ядро тоже
     * работает — по своему списку форм, — но разбор имён будет беднее.
     */
    private String prepareDictionary() {
        if (dictionary.isFile() && dictionary.length() >= DICTIONARY_MIN_BYTES) return "";
        File temporary = new File(dictionary.getAbsolutePath() + ".part");
        try {
            InputStream raw = getAssets().open(DICTIONARY_ASSET);
            GZIPInputStream packed = new GZIPInputStream(raw);
            OutputStream target = new FileOutputStream(temporary);
            try {
                byte[] buffer = new byte[64 * 1024];
                int read;
                while ((read = packed.read(buffer)) > 0) target.write(buffer, 0, read);
            } finally {
                target.close();
                packed.close();
            }
            if (!temporary.renameTo(dictionary)) throw new IOException("не удалось переименовать файл");
            return "Словарь форм подготовлен для следующих запусков.";
        } catch (IOException error) {
            temporary.delete();
            return "Словарь форм недоступен: " + error.getMessage();
        }
    }

    private static void writeText(File file, String text) throws IOException {
        Writer writer = new OutputStreamWriter(new FileOutputStream(file), "UTF-8");
        try {
            writer.write(text);
        } finally {
            writer.close();
        }
    }

    private static String readText(InputStream stream) throws IOException {
        ByteArrayOutputStream gathered = new ByteArrayOutputStream();
        try {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = stream.read(buffer)) > 0) gathered.write(buffer, 0, read);
        } finally {
            stream.close();
        }
        return new String(gathered.toByteArray(), "UTF-8");
    }
}

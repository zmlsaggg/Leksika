package org.leksika.sandbox;

import android.app.Activity;
import android.os.Bundle;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.ByteArrayInputStream;
import java.io.IOException;

/** Локальная учебная машина. В WebView нет доступа к сети, файлам и Java-мосту. */
public final class MainActivity extends Activity {
    private static final String ORIGIN = "appassets.androidplatform.net";
    private WebView view;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        view = new WebView(this);
        WebSettings settings = view.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        settings.setAllowFileAccessFromFileURLs(false);
        settings.setAllowUniversalAccessFromFileURLs(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        view.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView v, WebResourceRequest request) { return true; }
            @Override public WebResourceResponse shouldInterceptRequest(WebView v, WebResourceRequest request) {
                android.net.Uri uri = request.getUrl();
                String path = uri.getPath();
                if ("https".equals(uri.getScheme()) && ORIGIN.equals(uri.getHost()) && uri.getPort() == -1 &&
                    ("/index.html".equals(path) || "/editor.js".equals(path) ||
                     "/worker.js".equals(path) || "/machine.js".equals(path) || "/style.css".equals(path))) {
                    try {
                        String type = path.endsWith(".html") ? "text/html" : path.endsWith(".css") ? "text/css" : "application/javascript";
                        return new WebResourceResponse(type, "UTF-8", getAssets().open(path.substring(1)));
                    } catch (IOException ignored) { /* Return a closed response below. */ }
                }
                return new WebResourceResponse("text/plain", "UTF-8", 403, "Forbidden", null, new ByteArrayInputStream(new byte[0]));
            }
        });
        setContentView(view);
        // Проверка на эмуляторе включает этот флаг, чтобы не искать кнопку по
        // тексту на экране: разметка и прокрутка меняются, а мост остаётся.
        // В обычном запуске флага нет, и мост остаётся закрытым.
        final boolean автозапуск = getIntent().getBooleanExtra("автозапуск", false);
        if (автозапуск) view.addJavascriptInterface(new Object() {
            @android.webkit.JavascriptInterface
            public void запустить() {
                view.post(new Runnable() { @Override public void run() {
                    view.evaluateJavascript(
                        "var k=document.getElementById('run'); if(k){k.click();} else {document.title='нет-кнопки';}",
                        null);
                }});
            }
        }, "лексика_проверка");
        view.loadUrl("https://" + ORIGIN + "/index.html");
        if (автозапуск) view.postDelayed(new Runnable() { @Override public void run() {
            view.evaluateJavascript("лексика_проверка.запустить()", null);
        }}, 1500);
    }
    @Override protected void onDestroy() { if (view != null) view.destroy(); super.onDestroy(); }
}

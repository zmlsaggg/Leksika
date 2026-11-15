/* Google Tag Manager */
window.dataLayer = window.dataLayer || [];
  window.dataLayer.push({'gtm.start': new Date().getTime(), event: 'gtm.js'});
  (function(w,d,s,l,i){
    w[l]=w[l]||[];
    var f=d.getElementsByTagName(s)[0], j=d.createElement(s), dl=l!=='dataLayer'?'&l='+l:'';
    j.async=true;
    j.src='https://www.googletagmanager.com/gtm.js?id='+i+dl;
    f.parentNode.insertBefore(j,f);
  })(window,document,'script','dataLayer','GTM-K3CSQK5W');

/* Yandex.Metrika */
(function(m,e,t,r,i,k,a){
    m[i]=m[i]||function(){(m[i].a=m[i].a||[]).push(arguments)};
    m[i].l=1*new Date();
    for (var j=0; j<document.scripts.length; j++) {
      if (document.scripts[j].src === r) { return; }
    }
    k=e.createElement(t),a=e.getElementsByTagName(t)[0],k.async=1,k.src=r,a.parentNode.insertBefore(k,a);
  })(window, document, 'script', 'https://mc.yandex.ru/metrika/tag.js?id=114225735', 'ym');
  ym(114225735, 'init', {
    ssr:true,
    webvisor:true,
    clickmap:true,
    ecommerce:"dataLayer",
    referrer: document.referrer,
    url: location.href,
    accurateTrackBounce:true,
    trackLinks:true
  });

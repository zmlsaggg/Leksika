-- Выполняется после исходной схемы и миграции в отдельной тестовой базе.
insert into public.темы(id, автор, название, текст) values ('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa', '00000000-0000-4000-8000-000000000001', 'Тема', 'Текст');
select set_config('request.uid','00000000-0000-4000-8000-000000000001',false);
select set_config('request.jwt','{"app_metadata":{"provider":"github"}}',false);
set role authenticated;
do $$ begin if (select счёт from public.оценки_тем(array['aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'::uuid])) <> 0 then raise exception 'Начальная оценка не ноль'; end if; end $$;
select public.оценить_тему('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',1::smallint);
do $$ begin if (select счёт from public.оценки_тем(array['aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'::uuid])) <> 1 then raise exception 'Нет лайка'; end if; end $$;
select public.оценить_тему('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',1::smallint);
do $$ begin if (select счёт from public.оценки_тем(array['aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'::uuid])) <> 0 then raise exception 'Повтор не снял голос'; end if; end $$;
select public.оценить_тему('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',-1::smallint);
do $$ begin if (select счёт from public.оценки_тем(array['aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'::uuid])) <> -1 then raise exception 'Нет дизлайка'; end if; end $$;
insert into public.ответы_темы(тема, автор, текст) values ('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa', auth.uid(), 'Ответ');
do $$ begin
  begin insert into public.ответы_темы(тема, автор, текст) values ('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','00000000-0000-4000-8000-000000000002','Подмена'); raise exception 'Подмена автора разрешена'; exception when insufficient_privilege then null; end;
  begin insert into public.голоса_темы(тема, автор, оценка) values ('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',auth.uid(),1); raise exception 'Прямая запись голосов разрешена'; exception when insufficient_privilege then null; end;
end $$;
select set_config('request.jwt','{"app_metadata":{"provider":"email"}}',false);
do $$ begin
  begin perform public.оценить_тему('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',1::smallint); raise exception 'Голос не через GitHub разрешён'; exception when insufficient_privilege then null; end;
end $$;
reset role;
update public.темы set скрыто = true where id = 'aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa';
set role anon;
do $$ begin
  if (select count(*) from public.ответы_темы) <> 0 then raise exception 'Ответы скрытой темы видны'; end if;
  if (select count(*) from public.оценки_тем(array['aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa'::uuid])) <> 0 then raise exception 'Оценка скрытой темы видна'; end if;
end $$;
reset role;
